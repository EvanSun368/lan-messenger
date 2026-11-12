#include "node.h"
#include "socket_compat.h"
#include "e2e.h"
#include "transfer.h"

#include <algorithm>
#include <fstream>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace lan {
namespace {

using Clock = std::chrono::steady_clock;

in_addr parse_ipv4(const std::string &ip) {
	in_addr addr{};
	if (::inet_pton(AF_INET, ip.c_str(), &addr) != 1) {
		throw std::invalid_argument("invalid IPv4 address: " + ip);
	}
	return addr;
}

sockaddr_in make_addr(const std::string &ip, uint16_t port) {
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr = parse_ipv4(ip);
	return addr;
}

std::string addr_ip(const sockaddr_in &addr) {
	char text[INET_ADDRSTRLEN] = {};
	::inet_ntop(AF_INET, &addr.sin_addr, text, sizeof(text));
	return text;
}

uint16_t bound_port(Socket fd) {
	sockaddr_in addr{};
	socklen_t len = sizeof(addr);
	if (::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) < 0) {
		throw_socket_error("getsockname");
	}
	return ntohs(addr.sin_port);
}

Socket open_udp(const NodeConfig &config, uint16_t &bound) {
	const Socket fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (!valid_socket(fd)) {
		throw_socket_error("socket udp");
	}
	int yes = 1;
	set_sockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
	set_sockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
	uint16_t port = config.udp_port;
	if (!config.multicast_group.empty()) {
		port = config.discovery_port;
		if (port == 0) {
			Socket doomed = fd;
			close_socket(doomed);
			throw std::invalid_argument("multicast discovery requires a UDP port");
		}
	}
	auto addr = make_addr(config.bind_address, port);
	if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
		Socket doomed = fd;
		close_socket(doomed);
		throw_socket_error("bind udp");
	}
	if (!config.multicast_group.empty()) {
#ifdef _WIN32
		DWORD loop = 1;
		DWORD ttl = 1;
#else
		u_char loop = 1;
		u_char ttl = 1;
#endif
		set_sockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
		set_sockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
		if (config.bind_address != "0.0.0.0") {
			const auto iface = parse_ipv4(config.bind_address);
			set_sockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface));
		}
		ip_mreq membership{};
		membership.imr_multiaddr = parse_ipv4(config.multicast_group);
		membership.imr_interface = parse_ipv4(
			config.bind_address == "0.0.0.0" ? "0.0.0.0" : config.bind_address);
		if (set_sockopt(
				fd,
				IPPROTO_IP,
				IP_ADD_MEMBERSHIP,
				&membership,
				sizeof(membership)) < 0) {
			Socket doomed = fd;
			close_socket(doomed);
			throw_socket_error("IP_ADD_MEMBERSHIP");
		}
	}
	set_nonblock(fd);
	bound = bound_port(fd);
	return fd;
}

Socket open_listen(const NodeConfig &config, uint16_t &bound) {
	const Socket fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (!valid_socket(fd)) {
		throw_socket_error("socket tcp");
	}
	int yes = 1;
	set_sockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	auto addr = make_addr(config.bind_address, config.tcp_port);
	if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
		Socket doomed = fd;
		close_socket(doomed);
		throw_socket_error("bind tcp");
	}
	if (::listen(fd, 16) < 0) {
		Socket doomed = fd;
		close_socket(doomed);
		throw_socket_error("listen");
	}
	set_nonblock(fd);
	configure_tcp_socket(fd);
	bound = bound_port(fd);
	return fd;
}

} // namespace

struct Node::Impl {
	struct Connection {
		Socket fd = kInvalidSocket;
		bool connecting = false;
		bool hello_sent = false;
		bool hello_received = false;
		std::string peer_id;
		std::string display_name;
		std::vector<uint8_t> public_key;
		std::string ip;
		uint16_t tcp_port = 0;
		std::vector<uint8_t> read_buf;
		std::vector<uint8_t> write_buf;
		size_t write_off = 0;
	};

	static bool write_pending(const Connection &connection) {
		return connection.write_off < connection.write_buf.size();
	}

	void compact_write(Connection &connection) {
		if (connection.write_off == 0) {
			return;
		}
		if (connection.write_off >= connection.write_buf.size()) {
			connection.write_buf.clear();
			connection.write_off = 0;
			return;
		}
		// Wait until at least 1 MiB has been flushed (or half the
		// buffer) so a 2 MiB file frame is not memmove'd every 64 KiB.
		if (connection.write_off >= 1024 * 1024
			|| connection.write_off * 2 >= connection.write_buf.size()) {
			connection.write_buf.erase(
				connection.write_buf.begin(),
				connection.write_buf.begin() + static_cast<std::ptrdiff_t>(connection.write_off));
			connection.write_off = 0;
		}
	}

	bool needs_spin() const {
		// Spin only while there are bytes to write or a transfer is
		// actually moving. A connecting socket, a held folder file, or
		// an offer waiting for accept must not keep the GUI on a hot
		// timer.
		for (const auto &connection : connections_) {
			if (write_pending(connection)) {
				return true;
			}
		}
		for (const auto &file : outgoing_files_) {
			if (!file.finished && !file.held && file.offer_acked) {
				return true;
			}
		}
		for (const auto &file : incoming_files_) {
			if (!file.second.finished && file.second.accepted) {
				return true;
			}
		}
		return false;
	}

	struct Seen {
		PeerInfo info;
		Clock::time_point last_seen;
	};

	struct PendingText {
		std::string msg_id;
		uint64_t timestamp_ms = 0;
		std::string body;
		std::string group_id;
	};

	struct OutgoingFile {
		std::string transfer_id;
		std::string peer_id;
		std::string folder_id;
		std::string group_id;
		std::filesystem::path path;
		std::string filename;
		std::string mime;
		uint64_t size = 0;
		Sha256 sha256{};
		uint32_t chunk_count = 0;
		uint32_t next_to_send = 0;
		uint32_t acked_count = 0;
		bool offer_sent = false;
		bool offer_acked = false;
		bool held = false; // waiting for a folder ack
		bool finished = false;
		std::string batch_id;
		uint32_t last_persist = 0;
		uint32_t last_progress = 0;
		uint32_t chunk_size = kFileChunkPlainMax;
		bool encrypted = true;
		std::unique_ptr<std::ifstream> reader;
		uint32_t reader_index = 0;
		bool reader_ready = false;
		Sha256Hasher hasher;
		bool digest_sent = false;
		bool hasher_primed = false;
	};

	struct IncomingFile {
		std::string transfer_id;
		std::string peer_id;
		std::string folder_id;
		std::string group_id;
		std::string filename;
		std::string mime;
		uint64_t size = 0;
		Sha256 sha256{};
		uint32_t chunk_count = 0;
		uint32_t acked_count = 0;
		std::filesystem::path dest_path;
		std::filesystem::path temp_path;
		std::ofstream out;
		bool finished = false;
		uint32_t last_persist = 0;
		uint32_t last_progress = 0;
		uint32_t chunk_size = kFileChunkPlainMax;
		bool encrypted = true;
		Sha256Hasher hasher;
		bool hasher_primed = false;
		bool accepted = true;
	};

	struct OutgoingFolder {
		std::string folder_id;
		std::string peer_id;
		std::string group_id;
		std::string root;
		std::filesystem::path source;
		std::vector<std::string> empty_dirs;
		std::vector<std::string> file_ids;
		uint32_t file_count = 0;
		uint32_t files_done = 0;
		bool offer_sent = false;
		bool offer_acked = false;
		bool finished = false;
		std::string batch_id;
	};

	struct IncomingFolder {
		std::string folder_id;
		std::string peer_id;
		std::string group_id;
		std::string root;
		std::filesystem::path dest_root;
		std::vector<std::string> empty_dirs;
		uint32_t file_count = 0;
		uint32_t files_done = 0;
		bool finished = false;
		bool accepted = true;
	};

	explicit Impl(NodeConfig config)
		: config_(std::move(config)) {
		if (!is_valid_peer_id(config_.identity.peer_id)
			|| !is_valid_display_name(config_.identity.display_name)) {
			throw std::invalid_argument("invalid node identity");
		}
		if (!config_.e2e_key) {
			config_.e2e_key = std::make_shared<E2EKey>(E2EKey::generate());
		}
		if (config_.incoming_dir.empty()) {
			config_.incoming_dir = std::filesystem::temp_directory_path()
				/ "lanlink-incoming";
		}
		if (config_.state_dir.empty()) {
			config_.state_dir = config_.incoming_dir / ".state";
		}
	}

	std::filesystem::path transfers_dir() const {
		return config_.state_dir / "transfers";
	}

	std::filesystem::path groups_dir() const {
		return config_.state_dir / "groups";
	}

	Hello make_hello() const {
		Hello hello;
		hello.peer_id = config_.identity.peer_id;
		hello.display_name = config_.identity.display_name;
		hello.public_key = config_.e2e_key->public_key();
		hello.first_name = first_name_;
		hello.last_name = last_name_;
		hello.bio = bio_;
		return hello;
	}

	const std::vector<uint8_t> *lookup_public_key(
			const std::string &peer_id,
			const Connection *connection) const {
		if (connection && is_valid_public_key(connection->public_key)) {
			return &connection->public_key;
		}
		const auto seen = seen_.find(peer_id);
		if (seen != seen_.end() && is_valid_public_key(seen->second.info.public_key)) {
			return &seen->second.info.public_key;
		}
		return nullptr;
	}

	~Impl() {
		stop();
	}

	void start() {
		if (started_) {
			return;
		}
		ensure_sockets();
		udp_fd_ = open_udp(config_, udp_port_);
		listen_fd_ = open_listen(config_, tcp_port_);
		started_ = true;
		last_announce_ = Clock::time_point{};
		restore_state();
	}

	void stop() {
		for (auto &connection : connections_) {
			close_connection(connection);
		}
		connections_.clear();
		close_socket(listen_fd_);
		close_socket(udp_fd_);
		started_ = false;
	}

	void add_discovery_target(const std::string &ip, uint16_t port) {
		targets_.push_back(make_addr(ip, port));
	}

	void step(std::chrono::milliseconds timeout) {
		if (!started_) {
			throw std::runtime_error("node has not been started");
		}
		announce_if_due();
		expire_peers();
		try_connect_known_peers();

		std::vector<pollfd> fds;
		fds.push_back({udp_fd_, POLLIN, 0});
		fds.push_back({listen_fd_, POLLIN, 0});
		for (const auto &connection : connections_) {
			short events = POLLIN;
			if (connection.connecting || write_pending(connection)) {
				events = static_cast<short>(events | POLLOUT);
			}
			fds.push_back({connection.fd, events, 0});
		}
		const int ready = poll_sockets(fds.data(), static_cast<unsigned>(fds.size()), static_cast<int>(timeout.count()));
		if (ready < 0) {
			if (error_interrupted()) {
				return;
			}
			throw_socket_error("poll");
		}
		if (ready == 0) {
			pump_transfers();
			return;
		}
		if (fds[0].revents & POLLIN) {
			recv_udp();
		}
		if (fds[1].revents & POLLIN) {
			accept_tcp();
		}
		// accept_tcp() may append connections that were not in this poll set.
		const size_t polled_connections = fds.size() - 2;
		for (size_t i = 0; i < polled_connections; ++i) {
			const short revents = fds[i + 2].revents;
			if (revents != 0) {
				handle_connection(connections_[i], revents);
			}
		}
		connections_.erase(
			std::remove_if(
				connections_.begin(),
				connections_.end(),
				[](const Connection &connection) { return !valid_socket(connection.fd); }),
			connections_.end());
		pump_transfers();
	}


	FileEvent make_event(const OutgoingFile &file, FileEvent::Kind kind, std::string error = {}) {
		FileEvent event;
		event.kind = kind;
		event.outgoing = true;
		event.peer_id = file.peer_id;
		event.transfer_id = file.transfer_id;
		event.folder_id = file.folder_id;
		event.group_id = file.group_id;
		event.filename = file.filename;
		event.mime = file.mime;
		event.size = file.size;
		event.bytes_done = bytes_for_acked_chunks(file.size, file.acked_count);
		event.path = file.path;
		event.error = std::move(error);
		event.verified = (kind == FileEvent::Kind::Completed && file.sha256 != Sha256{});
		return event;
	}

	FileEvent make_event(const IncomingFile &file, FileEvent::Kind kind, std::string error = {}) {
		FileEvent event;
		event.kind = kind;
		event.outgoing = false;
		event.peer_id = file.peer_id;
		event.transfer_id = file.transfer_id;
		event.folder_id = file.folder_id;
		event.group_id = file.group_id;
		event.filename = file.filename;
		event.mime = file.mime;
		event.size = file.size;
		event.bytes_done = bytes_for_acked_chunks(file.size, file.acked_count);
		event.path = file.finished && !file.dest_path.empty() ? file.dest_path : file.temp_path;
		event.error = std::move(error);
		event.verified = (kind == FileEvent::Kind::Completed && file.sha256 != Sha256{});
		return event;
	}

	FileEvent make_folder_event(
			const std::string &peer_id,
			const std::string &folder_id,
			const std::string &group_id,
			const std::string &root,
			const std::filesystem::path &path,
			uint32_t files_total,
			uint32_t files_done,
			bool outgoing,
			FileEvent::Kind kind,
			std::string error = {}) {
		FileEvent event;
		event.kind = kind;
		event.outgoing = outgoing;
		event.is_folder = true;
		event.peer_id = peer_id;
		event.transfer_id = folder_id;
		event.folder_id = folder_id;
		event.group_id = group_id;
		event.filename = root;
		event.path = path;
		event.files_total = files_total;
		event.files_done = files_done;
		event.error = std::move(error);
		return event;
	}

	void forget_transfer(const std::string &transfer_id) {
		remove_transfer_state(transfers_dir(), transfer_id);
	}

	TransferState snapshot(const OutgoingFile &file) const {
		TransferState state;
		state.outgoing = true;
		state.transfer_id = file.transfer_id;
		state.peer_id = file.peer_id;
		state.folder_id = file.folder_id;
		state.group_id = file.group_id;
		state.path = file.path;
		state.filename = file.filename;
		state.mime = file.mime;
		state.size = file.size;
		state.sha256 = file.sha256;
		state.chunk_count = file.chunk_count;
		state.acked_count = file.acked_count;
		state.chunk_size = file.chunk_size;
		state.encrypted = file.encrypted;
		state.finished = file.finished;
		return state;
	}

	TransferState snapshot(const IncomingFile &file) const {
		TransferState state;
		state.outgoing = false;
		state.transfer_id = file.transfer_id;
		state.peer_id = file.peer_id;
		state.folder_id = file.folder_id;
		state.group_id = file.group_id;
		state.path = file.temp_path;
		state.filename = file.filename;
		state.mime = file.mime;
		state.size = file.size;
		state.sha256 = file.sha256;
		state.chunk_count = file.chunk_count;
		state.acked_count = file.acked_count;
		state.chunk_size = file.chunk_size;
		state.encrypted = file.encrypted;
		state.finished = file.finished;
		return state;
	}

	void persist(const OutgoingFile &file) {
		if (file.finished) {
			forget_transfer(file.transfer_id);
			return;
		}
		try {
			save_transfer_state(transfers_dir(), snapshot(file));
		} catch (...) {
		}
	}

	void persist(const IncomingFile &file) {
		if (file.finished) {
			forget_transfer(file.transfer_id);
			return;
		}
		try {
			save_transfer_state(transfers_dir(), snapshot(file));
		} catch (...) {
		}
	}

	void fail_outgoing(OutgoingFile &file, const std::string &error) {
		if (file.finished) {
			return;
		}
		file.finished = true;
		forget_transfer(file.transfer_id);
		file_events_.push_back(make_event(file, FileEvent::Kind::Failed, error));
		if (auto *connection = find_connection(file.peer_id)) {
			queue_frame(*connection, FileAbort{file.transfer_id, 1});
		}
	}

	void fail_incoming(IncomingFile &file, const std::string &error, uint8_t reason) {
		if (file.finished) {
			return;
		}
		file.finished = true;
		file.out.close();
		if (!file.temp_path.empty()) {
			std::error_code ec;
			std::filesystem::remove(file.temp_path, ec);
		}
		forget_transfer(file.transfer_id);
		file_events_.push_back(make_event(file, FileEvent::Kind::Failed, error));
		if (auto *connection = find_connection(file.peer_id)) {
			queue_frame(*connection, FileAbort{file.transfer_id, reason});
		}
	}

	void pause_outgoing(OutgoingFile &file) {
		if (file.finished) {
			return;
		}
		file.offer_sent = false;
		file.offer_acked = false;
		file.next_to_send = file.acked_count;
		file.reader.reset();
		persist(file);
	}

	void pause_incoming(IncomingFile &file) {
		if (file.finished) {
			return;
		}
		file.out.close();
		std::error_code ec;
		if (!file.temp_path.empty() && std::filesystem::exists(file.temp_path, ec)) {
			const auto part_size = std::filesystem::file_size(file.temp_path, ec);
			if (!ec) {
				file.acked_count = acked_count_from_part_size(file.size, part_size);
				const auto keep = bytes_for_acked_chunks(file.size, file.acked_count);
				if (part_size > keep) {
					std::filesystem::resize_file(file.temp_path, keep, ec);
				}
			}
		}
		persist(file);
	}

	void pause_peer_transfers(const std::string &peer_id) {
		for (auto &file : outgoing_files_) {
			if (file.peer_id == peer_id) {
				pause_outgoing(file);
			}
		}
		for (auto &entry : incoming_files_) {
			if (entry.second.peer_id == peer_id) {
				pause_incoming(entry.second);
			}
		}
		for (auto &folder : outgoing_folders_) {
			if (folder.peer_id == peer_id && !folder.finished) {
				folder.offer_sent = false;
				folder.offer_acked = false;
			}
		}
	}

	void close_connection(Connection &connection) {
		if (connection.hello_received && !connection.peer_id.empty()) {
			pause_peer_transfers(connection.peer_id);
		}
		close_socket(connection.fd);
	}

	std::vector<uint8_t> seal_for(
			const std::string &peer_id,
			Connection *connection,
			std::string_view plain) {
		const auto *key = lookup_public_key(peer_id, connection);
		if (!key || !config_.e2e_key) {
			throw std::runtime_error("missing peer key");
		}
		return config_.e2e_key->encrypt_for(*key, plain);
	}

	std::optional<std::string> open_from(
			const std::string &peer_id,
			Connection *connection,
			const std::vector<uint8_t> &sealed) {
		const auto *key = lookup_public_key(peer_id, connection);
		if (!key || !config_.e2e_key) {
			return std::nullopt;
		}
		return config_.e2e_key->decrypt_from(
			*key,
			std::string_view(
				reinterpret_cast<const char *>(sealed.data()),
				sealed.size()));
	}

	std::optional<std::string> unpack_payload(
			const std::string &peer_id,
			Connection *connection,
			const std::vector<uint8_t> &payload,
			bool *was_plain = nullptr) {
		if (is_plaintext_payload(payload.data(), payload.size())) {
			const auto inner = unwrap_plaintext_payload(payload.data(), payload.size());
			if (!inner) {
				return std::nullopt;
			}
			if (was_plain) {
				*was_plain = true;
			}
			return std::string(
				reinterpret_cast<const char *>(inner->data()),
				inner->size());
		}
		if (was_plain) {
			*was_plain = false;
		}
		return open_from(peer_id, connection, payload);
	}

	void send_offer(OutgoingFile &file) {
		auto *connection = find_connection(file.peer_id);
		if (!connection) {
			return;
		}
		FileOfferPlain meta;
		meta.filename = file.filename;
		meta.mime = file.mime;
		meta.sha256 = file.sha256;
		meta.folder_id = file.folder_id;
		meta.group_id = file.group_id;
		const auto plain = encode_offer_plain(meta);
		FileOffer offer;
		offer.transfer_id = file.transfer_id;
		offer.size = file.size;
		offer.chunk_count = file.chunk_count;
		if (file.encrypted) {
			offer.sealed = seal_for(
				file.peer_id,
				connection,
				std::string_view(
					reinterpret_cast<const char *>(plain.data()),
					plain.size()));
		} else {
			offer.sealed = encode_plain_file_offer(file.chunk_size, meta);
		}
		queue_frame(*connection, offer);
		file.offer_sent = true;
		persist(file);
		file_events_.push_back(make_event(file, FileEvent::Kind::Offered));
	}

	bool should_flush_state(uint32_t acked, uint32_t total, uint32_t last, bool force) {
		return force
			|| acked == total
			|| last == 0
			|| acked - last >= 8;
	}

	void maybe_persist_outgoing(OutgoingFile &file, bool force) {
		if (should_flush_state(file.acked_count, file.chunk_count, file.last_persist, force)) {
			persist(file);
			file.last_persist = file.acked_count;
		}
	}

	void maybe_persist_incoming(IncomingFile &file, bool force) {
		if (should_flush_state(file.acked_count, file.chunk_count, file.last_persist, force)) {
			persist(file);
			file.last_persist = file.acked_count;
		}
	}

	void emit_outgoing_progress(OutgoingFile &file, bool force) {
		if (should_flush_state(file.acked_count, file.chunk_count, file.last_progress, force)) {
			file_events_.push_back(make_event(file, FileEvent::Kind::Progress));
			file.last_progress = file.acked_count;
		}
	}

	void emit_incoming_progress(IncomingFile &file, bool force) {
		if (should_flush_state(file.acked_count, file.chunk_count, file.last_progress, force)) {
			file_events_.push_back(make_event(file, FileEvent::Kind::Progress));
			file.last_progress = file.acked_count;
		}
	}

	void send_chunks(OutgoingFile &file) {
		auto *connection = find_connection(file.peer_id);
		if (!connection || !file.offer_acked || file.finished) {
			return;
		}
		const auto step = effective_chunk_size(file.chunk_size);
		if (!file.reader) {
			file.reader = std::make_unique<std::ifstream>(file.path, std::ios::binary);
			if (!*file.reader) {
				fail_outgoing(file, "unable to read file");
				return;
			}
			file.reader_ready = false;
		}
		if (!file.hasher_primed) {
			try {
				if (file.acked_count > 0 && !file.hasher.finished()) {
					const auto already = bytes_for_acked_chunks(
						file.size, file.acked_count, file.chunk_size);
					file.hasher.update_file_prefix(file.path, already);
				}
				file.hasher_primed = true;
			} catch (const std::exception &error) {
				fail_outgoing(file, error.what());
				return;
			}
		}
		while (file.next_to_send < file.chunk_count
			&& file.next_to_send - file.acked_count < kFileWindow) {
			const auto index = file.next_to_send;
			const auto n = file_chunk_plain_size(file.size, index, step);
			std::string plain(n, '\0');
			if (n > 0) {
				const bool sequential = file.reader_ready && file.reader_index == index;
				if (!sequential) {
					file.reader->clear();
					file.reader->seekg(static_cast<std::streamoff>(index) * step);
				}
				file.reader->read(plain.data(), static_cast<std::streamsize>(n));
				if (file.reader->gcount() != static_cast<std::streamsize>(n)) {
					fail_outgoing(file, "short file read");
					return;
				}
				file.reader_index = index + 1;
				file.reader_ready = true;
			}
			if (n > 0 && !file.hasher.finished()) {
				file.hasher.update(plain.data(), plain.size());
			}
			FileChunk chunk;
			chunk.transfer_id = file.transfer_id;
			chunk.index = index;
			try {
				if (file.encrypted) {
					chunk.sealed = seal_for(file.peer_id, connection, plain);
				} else {
					chunk.sealed.assign(
						reinterpret_cast<const uint8_t *>(plain.data()),
						reinterpret_cast<const uint8_t *>(plain.data()) + plain.size());
				}
			} catch (const std::exception &error) {
				fail_outgoing(file, error.what());
				return;
			}
			queue_frame(*connection, chunk);
			++file.next_to_send;
		}
		if (!file.digest_sent && file.next_to_send == file.chunk_count) {
			send_digest(file);
		}
	}

	void send_digest(OutgoingFile &file) {
		if (file.digest_sent || file.finished) {
			return;
		}
		auto *connection = find_connection(file.peer_id);
		if (!connection) {
			return;
		}
		try {
			if (file.sha256 == Sha256{}) {
				file.sha256 = file.hasher.finish();
			} else if (!file.hasher.finished() && file.hasher_primed) {
				const auto live = file.hasher.finish();
				if (live != file.sha256) {
					fail_outgoing(file, "checksum mismatch");
					return;
				}
			}
		} catch (const std::exception &error) {
			fail_outgoing(file, error.what());
			return;
		}
		Sha256 digest = file.sha256;
		if (forced_digest_) {
			digest = *forced_digest_;
			forced_digest_.reset();
		}
		FileDigest frame;
		frame.transfer_id = file.transfer_id;
		frame.sha256.assign(digest.begin(), digest.end());
		queue_frame(*connection, frame);
		file.digest_sent = true;
		persist(file);
	}

	void pump_transfers() {
		for (auto &file : outgoing_files_) {
			if (file.finished) {
				continue;
			}
			try {
				if (file.held) {
					continue;
				}
				if (!file.offer_sent) {
					if (find_connection(file.peer_id)) {
						send_offer(file);
					}
				} else if (file.offer_acked) {
					send_chunks(file);
				}
			} catch (const std::exception &error) {
				fail_outgoing(file, error.what());
			}
		}
		pump_folders();
	}

	OutgoingFile *find_outgoing(const std::string &transfer_id) {
		for (auto &file : outgoing_files_) {
			if (file.transfer_id == transfer_id) {
				return &file;
			}
		}
		return nullptr;
	}

	std::optional<std::filesystem::path> incoming_dest_for(
			const std::string &folder_id,
			const std::string &filename) const {
		if (folder_id.empty()) {
			return unique_destination(config_.incoming_dir, sanitize_filename(filename));
		}
		auto folder = incoming_folders_.find(folder_id);
		if (folder == incoming_folders_.end() || folder->second.finished) {
			return std::nullopt;
		}
		return resolve_under_root(folder->second.dest_root, filename);
	}

	bool open_incoming_part(IncomingFile &incoming, bool resume) {
		if (incoming.temp_path.empty()) {
			return false;
		}
		std::filesystem::create_directories(incoming.temp_path.parent_path());
		if (resume) {
			std::error_code ec;
			uint64_t part_size = 0;
			if (std::filesystem::exists(incoming.temp_path, ec)) {
				part_size = std::filesystem::file_size(incoming.temp_path, ec);
				if (ec) {
					part_size = 0;
				}
			}
			incoming.acked_count = acked_count_from_part_size(
				incoming.size, part_size, incoming.chunk_size);
			const auto keep = bytes_for_acked_chunks(
				incoming.size, incoming.acked_count, incoming.chunk_size);
			if (part_size != keep) {
				std::filesystem::resize_file(incoming.temp_path, keep, ec);
			}
			incoming.out.open(incoming.temp_path, std::ios::binary | std::ios::in | std::ios::out);
			if (incoming.out) {
				incoming.out.seekp(static_cast<std::streamoff>(keep));
			}
		} else {
			incoming.out.open(incoming.temp_path, std::ios::binary | std::ios::trunc);
			incoming.acked_count = 0;
		}
		if (incoming.out) {
			incoming.hasher = Sha256Hasher{};
			incoming.hasher_primed = false;
			if (incoming.acked_count > 0) {
				const auto keep = bytes_for_acked_chunks(
					incoming.size, incoming.acked_count, incoming.chunk_size);
				incoming.hasher.update_file_prefix(incoming.temp_path, keep);
			}
			incoming.hasher_primed = true;
		}
		return static_cast<bool>(incoming.out);
	}

	void on_file_offer(Connection &connection, const FileOffer &offer) {
		if (offer.size > kMaxFileSize || offer.chunk_count == 0) {
			queue_frame(connection, FileAbort{offer.transfer_id, 3});
			return;
		}
		FileOfferPlain meta;
		uint32_t chunk_size = kFileChunkPlainMax;
		bool encrypted = true;
		if (is_plaintext_payload(offer.sealed.data(), offer.sealed.size())) {
			const auto plain = decode_plain_file_offer(offer.sealed.data(), offer.sealed.size());
			if (!plain) {
				queue_frame(connection, FileAbort{offer.transfer_id, 1});
				return;
			}
			meta = plain->meta;
			chunk_size = plain->chunk_size;
			encrypted = false;
		} else {
			const auto opened = open_from(connection.peer_id, &connection, offer.sealed);
			if (!opened) {
				queue_frame(connection, FileAbort{offer.transfer_id, 1});
				return;
			}
			const auto decoded = decode_offer_plain(
				reinterpret_cast<const uint8_t *>(opened->data()),
				opened->size());
			if (!decoded) {
				queue_frame(connection, FileAbort{offer.transfer_id, 1});
				return;
			}
			meta = *decoded;
			encrypted = true;
			chunk_size = kFileChunkPlainMax;
		}
		if (offer.chunk_count != file_chunk_count(offer.size, chunk_size)) {
			queue_frame(connection, FileAbort{offer.transfer_id, 3});
			return;
		}
		TransferState offered;
		offered.transfer_id = offer.transfer_id;
		offered.peer_id = connection.peer_id;
		offered.folder_id = meta.folder_id;
		offered.group_id = meta.group_id;
		offered.size = offer.size;
		offered.sha256 = meta.sha256;
		offered.chunk_count = offer.chunk_count;
		offered.chunk_size = chunk_size;
		offered.encrypted = encrypted;

		auto existing = incoming_files_.find(offer.transfer_id);
		if (existing != incoming_files_.end()) {
			auto &file = existing->second;
			if (!file.accepted) {
				return;
			}
			if (file.finished) {
				queue_frame(connection, FileAck{offer.transfer_id, file.chunk_count});
				queue_frame(connection, FileComplete{offer.transfer_id});
				return;
			}
			if (!transfer_can_resume(snapshot(file), offered)) {
				fail_incoming(file, "resume mismatch", 2);
				return;
			}
			file.out.close();
			if (!open_incoming_part(file, true)) {
				fail_incoming(file, "unable to resume temp file", 4);
				return;
			}
			persist(file);
			queue_frame(connection, FileAck{file.transfer_id, file.acked_count});
			return;
		}

		IncomingFile incoming;
		incoming.transfer_id = offer.transfer_id;
		incoming.peer_id = connection.peer_id;
		incoming.folder_id = meta.folder_id;
		incoming.group_id = meta.group_id;
		incoming.filename = meta.folder_id.empty()
			? sanitize_filename(meta.filename)
			: meta.filename;
		incoming.mime = meta.mime;
		incoming.size = offer.size;
		incoming.sha256 = meta.sha256;
		incoming.chunk_count = offer.chunk_count;
		incoming.chunk_size = chunk_size;
		incoming.encrypted = encrypted;
		incoming.accepted = config_.auto_accept_incoming;
		if (!incoming.folder_id.empty()) {
			auto folder = incoming_folders_.find(incoming.folder_id);
			if (folder == incoming_folders_.end() || folder->second.finished || !folder->second.accepted) {
				queue_frame(connection, FileAbort{offer.transfer_id, 3});
				return;
			}
		}
		if (!incoming.accepted) {
			file_events_.push_back(make_event(incoming, FileEvent::Kind::Offered));
			incoming_files_[offer.transfer_id] = std::move(incoming);
			return;
		}
		if (!begin_incoming_file(connection, incoming)) {
			return;
		}
		auto &stored = incoming_files_[offer.transfer_id] = std::move(incoming);
		persist(stored);
		queue_frame(connection, FileAck{stored.transfer_id, stored.acked_count});
	}

	bool begin_incoming_file(Connection &connection, IncomingFile &incoming) {
		std::optional<std::filesystem::path> dest;
		if (!incoming.dest_path.empty()) {
			dest = incoming.dest_path;
		} else {
			dest = incoming_dest_for(incoming.folder_id, incoming.filename);
		}
		if (!dest) {
			queue_frame(connection, FileAbort{incoming.transfer_id, 3});
			return false;
		}
		incoming.dest_path = *dest;
		incoming.temp_path = incoming.dest_path;
		incoming.temp_path += ".part";
		incoming.accepted = true;
		std::filesystem::create_directories(incoming.dest_path.parent_path());
		if (!open_incoming_part(incoming, std::filesystem::exists(incoming.temp_path))) {
			file_events_.push_back(make_event(incoming, FileEvent::Kind::Failed, "unable to create temp file"));
			queue_frame(connection, FileAbort{incoming.transfer_id, 4});
			return false;
		}
		file_events_.push_back(make_event(incoming, FileEvent::Kind::Started));
		return true;
	}

	void on_file_chunk(Connection &connection, const FileChunk &chunk) {
		auto it = incoming_files_.find(chunk.transfer_id);
		if (it == incoming_files_.end() || it->second.finished || !it->second.accepted) {
			return;
		}
		auto &file = it->second;
		if (chunk.index < file.acked_count) {
			queue_frame(connection, FileAck{file.transfer_id, file.acked_count});
			return;
		}
		if (chunk.index != file.acked_count) {
			fail_incoming(file, "out-of-order chunk", 3);
			return;
		}
		std::string opened;
		if (file.encrypted) {
			if (is_plaintext_payload(chunk.sealed.data(), chunk.sealed.size())) {
				fail_incoming(file, "plaintext chunk on encrypted transfer", 1);
				return;
			}
			const auto decrypted = open_from(connection.peer_id, &connection, chunk.sealed);
			if (!decrypted) {
				fail_incoming(file, "decrypt failed", 1);
				return;
			}
			opened = *decrypted;
		} else {
			opened.assign(
				reinterpret_cast<const char *>(chunk.sealed.data()),
				chunk.sealed.size());
		}
		const auto expected = file_chunk_plain_size(file.size, chunk.index, file.chunk_size);
		if (opened.size() != expected) {
			fail_incoming(file, "unexpected chunk size", 3);
			return;
		}
		if (expected > 0) {
			file.out.write(opened.data(), static_cast<std::streamsize>(expected));
			if (!file.out) {
				fail_incoming(file, "write failed", 4);
				return;
			}
			if (!file.hasher.finished()) {
				file.hasher.update(opened.data(), opened.size());
			}
		}
		++file.acked_count;
		maybe_persist_incoming(file, file.acked_count == file.chunk_count);
		const bool ack_now = file.encrypted
			|| file.acked_count == file.chunk_count
			|| (file.acked_count % kPlainAckEvery) == 0;
		if (ack_now) {
			queue_frame(connection, FileAck{file.transfer_id, file.acked_count});
		}
		emit_incoming_progress(file, file.acked_count == file.chunk_count);
		try_complete_incoming(connection, file);
	}

	void on_file_digest(Connection &connection, const FileDigest &frame) {
		auto it = incoming_files_.find(frame.transfer_id);
		if (it == incoming_files_.end() || it->second.finished) {
			return;
		}
		if (frame.sha256.size() != kSha256Size) {
			fail_incoming(it->second, "checksum mismatch", 2);
			return;
		}
		Sha256 digest{};
		std::copy(frame.sha256.begin(), frame.sha256.end(), digest.begin());
		if (it->second.sha256 != Sha256{} && it->second.sha256 != digest) {
			fail_incoming(it->second, "checksum mismatch", 2);
			return;
		}
		it->second.sha256 = digest;
		try_complete_incoming(connection, it->second);
	}

	void try_complete_incoming(Connection &connection, IncomingFile &file) {
		if (file.finished || !file.accepted || file.acked_count != file.chunk_count) {
			return;
		}
		if (file.sha256 == Sha256{}) {
			// Wait for the sender's FileDigest trailer. Encrypted
			// offers already carry the digest, so this only applies
			// to the streaming plaintext path.
			return;
		}
		file.out.close();
		try {
			const auto digest = file.hasher.finish();
			if (digest != file.sha256) {
				fail_incoming(file, "checksum mismatch", 2);
				return;
			}
		} catch (const std::exception &error) {
			fail_incoming(file, error.what(), 2);
			return;
		}
		auto dest = file.dest_path;
		if (dest.empty()) {
			dest = unique_destination(config_.incoming_dir, file.filename);
		} else {
			std::filesystem::create_directories(dest.parent_path());
		}
		std::error_code ec;
		if (std::filesystem::exists(dest, ec)) {
			std::filesystem::remove(dest, ec);
		}
		std::filesystem::rename(file.temp_path, dest, ec);
		if (ec) {
			fail_incoming(file, "rename failed", 4);
			return;
		}
		file.dest_path = dest;
		file.temp_path = dest;
		file.finished = true;
		forget_transfer(file.transfer_id);
		file_events_.push_back(make_event(file, FileEvent::Kind::Completed));
		queue_frame(connection, FileComplete{file.transfer_id});
		note_incoming_folder_file(connection, file);
	}

	void on_file_ack(const FileAck &ack) {
		auto *file = find_outgoing(ack.transfer_id);
		if (!file || file->finished) {
			return;
		}
		if (ack.acked_count > file->chunk_count) {
			return;
		}
		if (!file->offer_acked) {
			file->offer_acked = true;
			file->acked_count = ack.acked_count;
			file->next_to_send = ack.acked_count;
			maybe_persist_outgoing(*file, true);
			file_events_.push_back(make_event(*file, FileEvent::Kind::Started));
			if (file->acked_count == file->chunk_count) {
				return;
			}
			send_chunks(*file);
			return;
		}
		if (ack.acked_count < file->acked_count
			|| ack.acked_count > file->next_to_send) {
			return;
		}
		file->acked_count = ack.acked_count;
		maybe_persist_outgoing(*file, file->acked_count == file->chunk_count);
		emit_outgoing_progress(*file, file->acked_count == file->chunk_count);
		send_chunks(*file);
	}

	void on_file_complete(const FileComplete &done) {
		auto *file = find_outgoing(done.transfer_id);
		if (!file || file->finished) {
			return;
		}
		if (file->acked_count != file->chunk_count) {
			fail_outgoing(*file, "complete before all chunks were acked");
			return;
		}
		file->finished = true;
		forget_transfer(file->transfer_id);
		file_events_.push_back(make_event(*file, FileEvent::Kind::Completed));
		note_outgoing_folder_file(*file);
	}

	void on_file_abort(Connection &connection, const FileAbort &abort) {
		if (auto *file = find_outgoing(abort.transfer_id)) {
			const bool declined = !file->offer_acked
				|| abort.reason == kAbortRejected;
			fail_outgoing(*file, declined ? "rejected" : "peer aborted the transfer");
		}
		auto it = incoming_files_.find(abort.transfer_id);
		if (it != incoming_files_.end()) {
			// Do not send another abort back.
			if (!it->second.finished) {
				it->second.finished = true;
				it->second.out.close();
				std::error_code ec;
				std::filesystem::remove(it->second.temp_path, ec);
				forget_transfer(it->second.transfer_id);
				file_events_.push_back(make_event(
					it->second,
					FileEvent::Kind::Failed,
					"peer aborted the transfer"));
			}
		}
		(void)connection;
	}


	void restore_state() {
		for (auto &state : load_all_transfer_states(transfers_dir())) {
			if (state.finished || !is_valid_peer_id(state.transfer_id)) {
				forget_transfer(state.transfer_id);
				continue;
			}
			if (state.outgoing) {
				std::error_code ec;
				if (!std::filesystem::is_regular_file(state.path, ec)) {
					forget_transfer(state.transfer_id);
					continue;
				}
				OutgoingFile file;
				file.transfer_id = state.transfer_id;
				file.peer_id = state.peer_id;
				file.folder_id = state.folder_id;
				file.group_id = state.group_id;
				file.path = state.path;
				file.filename = state.filename;
				file.mime = state.mime;
				file.size = state.size;
				file.sha256 = state.sha256;
				file.chunk_count = state.chunk_count;
				file.acked_count = state.acked_count;
				file.chunk_size = effective_chunk_size(state.chunk_size);
				file.encrypted = state.encrypted;
				file.next_to_send = state.acked_count;
				file.held = !state.folder_id.empty();
				outgoing_files_.push_back(std::move(file));
			} else {
				IncomingFile file;
				file.transfer_id = state.transfer_id;
				file.peer_id = state.peer_id;
				file.folder_id = state.folder_id;
				file.group_id = state.group_id;
				file.filename = state.filename;
				file.mime = state.mime;
				file.size = state.size;
				file.sha256 = state.sha256;
				file.chunk_count = state.chunk_count;
				file.acked_count = state.acked_count;
				file.chunk_size = effective_chunk_size(state.chunk_size);
				file.encrypted = state.encrypted;
				file.temp_path = state.path;
				auto dest = state.path;
				if (dest.extension() == ".part") {
					dest.replace_extension();
					// replace_extension() drops only ".part" when the stem
					// has no extra extension; filenames like "a.txt.part"
					// become "a.txt".
				}
				file.dest_path = dest;
				incoming_files_[file.transfer_id] = std::move(file);
			}
		}
		for (auto &group : load_all_groups(groups_dir())) {
			groups_[group.group_id] = group;
		}
	}

	OutgoingFolder *find_outgoing_folder(const std::string &folder_id) {
		for (auto &folder : outgoing_folders_) {
			if (folder.folder_id == folder_id) {
				return &folder;
			}
		}
		return nullptr;
	}

	void send_folder_offer(OutgoingFolder &folder) {
		auto *connection = find_connection(folder.peer_id);
		if (!connection) {
			return;
		}
		FolderOfferPlain meta;
		meta.root = folder.root;
		meta.file_count = folder.file_count;
		meta.empty_dirs = folder.empty_dirs;
		meta.group_id = folder.group_id;
		const auto plain = encode_folder_offer_plain(meta);
		FolderOffer offer;
		offer.folder_id = folder.folder_id;
		offer.file_count = folder.file_count;
		if (config_.encrypt_files) {
			offer.sealed = seal_for(
				folder.peer_id,
				connection,
				std::string_view(
					reinterpret_cast<const char *>(plain.data()),
					plain.size()));
		} else {
			offer.sealed = wrap_plaintext_payload(plain.data(), plain.size());
		}
		queue_frame(*connection, offer);
		folder.offer_sent = true;
		file_events_.push_back(make_folder_event(
			folder.peer_id,
			folder.folder_id,
			folder.group_id,
			folder.root,
			folder.source,
			folder.file_count,
			folder.files_done,
			true,
			FileEvent::Kind::Offered));
	}

	void release_folder_files(OutgoingFolder &folder) {
		for (const auto &id : folder.file_ids) {
			if (auto *file = find_outgoing(id)) {
				file->held = false;
			}
		}
	}

	void pump_folders() {
		for (auto &folder : outgoing_folders_) {
			if (folder.finished) {
				continue;
			}
			try {
				if (!folder.offer_sent) {
					if (find_connection(folder.peer_id)) {
						send_folder_offer(folder);
					}
				}
			} catch (const std::exception &error) {
				fail_outgoing_folder(folder, error.what());
			}
		}
	}

	void fail_outgoing_folder(OutgoingFolder &folder, const std::string &error) {
		if (folder.finished) {
			return;
		}
		folder.finished = true;
		for (const auto &id : folder.file_ids) {
			if (auto *file = find_outgoing(id)) {
				fail_outgoing(*file, error);
			}
		}
		if (auto *connection = find_connection(folder.peer_id)) {
			queue_frame(*connection, FolderAbort{folder.folder_id, kAbortRejected});
		}
		file_events_.push_back(make_folder_event(
			folder.peer_id,
			folder.folder_id,
			folder.group_id,
			folder.root,
			folder.source,
			folder.file_count,
			folder.files_done,
			true,
			FileEvent::Kind::Failed,
			error));
	}

	void note_outgoing_folder_file(const OutgoingFile &file) {
		if (file.folder_id.empty()) {
			return;
		}
		auto *folder = find_outgoing_folder(file.folder_id);
		if (!folder || folder->finished) {
			return;
		}
		++folder->files_done;
		file_events_.push_back(make_folder_event(
			folder->peer_id,
			folder->folder_id,
			folder->group_id,
			folder->root,
			folder->source,
			folder->file_count,
			folder->files_done,
			true,
			FileEvent::Kind::Progress));
		if (folder->files_done >= folder->file_count) {
			if (auto *connection = find_connection(folder->peer_id)) {
				queue_frame(*connection, FolderComplete{folder->folder_id});
			}
			folder->finished = true;
			file_events_.push_back(make_folder_event(
				folder->peer_id,
				folder->folder_id,
				folder->group_id,
				folder->root,
				folder->source,
				folder->file_count,
				folder->files_done,
				true,
				FileEvent::Kind::Completed));
		}
	}

	void note_incoming_folder_file(Connection &connection, const IncomingFile &file) {
		if (file.folder_id.empty()) {
			return;
		}
		auto it = incoming_folders_.find(file.folder_id);
		if (it == incoming_folders_.end() || it->second.finished) {
			return;
		}
		auto &folder = it->second;
		++folder.files_done;
		file_events_.push_back(make_folder_event(
			folder.peer_id,
			folder.folder_id,
			folder.group_id,
			folder.root,
			folder.dest_root,
			folder.file_count,
			folder.files_done,
			false,
			FileEvent::Kind::Progress));
		if (folder.files_done >= folder.file_count) {
			complete_incoming_folder(connection, folder);
		}
	}

	void complete_incoming_folder(Connection &connection, IncomingFolder &folder) {
		if (folder.finished) {
			return;
		}
		for (const auto &dir : folder.empty_dirs) {
			if (const auto path = resolve_under_root(folder.dest_root, dir)) {
				std::filesystem::create_directories(*path);
			}
		}
		std::filesystem::create_directories(folder.dest_root);
		folder.finished = true;
		file_events_.push_back(make_folder_event(
			folder.peer_id,
			folder.folder_id,
			folder.group_id,
			folder.root,
			folder.dest_root,
			folder.file_count,
			folder.files_done,
			false,
			FileEvent::Kind::Completed));
		queue_frame(connection, FolderComplete{folder.folder_id});
	}

	void on_folder_offer(Connection &connection, const FolderOffer &offer) {
		bool was_plain = false;
		const auto opened = unpack_payload(
			connection.peer_id, &connection, offer.sealed, &was_plain);
		if (!opened) {
			queue_frame(connection, FolderAbort{offer.folder_id, 1});
			return;
		}
		const auto meta = decode_folder_offer_plain(
			reinterpret_cast<const uint8_t *>(opened->data()),
			opened->size());
		if (!meta || meta->file_count != offer.file_count) {
			queue_frame(connection, FolderAbort{offer.folder_id, 3});
			return;
		}
		auto existing = incoming_folders_.find(offer.folder_id);
		if (existing != incoming_folders_.end()) {
			if (!existing->second.accepted) {
				return;
			}
			queue_frame(connection, FolderAck{offer.folder_id, existing->second.files_done});
			if (existing->second.finished) {
				queue_frame(connection, FolderComplete{offer.folder_id});
			}
			return;
		}
		IncomingFolder folder;
		folder.folder_id = offer.folder_id;
		folder.peer_id = connection.peer_id;
		folder.group_id = meta->group_id;
		folder.root = meta->root;
		folder.empty_dirs = meta->empty_dirs;
		folder.file_count = offer.file_count;
		folder.accepted = config_.auto_accept_incoming;
		if (!folder.accepted) {
			file_events_.push_back(make_folder_event(
				folder.peer_id,
				folder.folder_id,
				folder.group_id,
				folder.root,
				{},
				folder.file_count,
				0,
				false,
				FileEvent::Kind::Offered));
			incoming_folders_[offer.folder_id] = std::move(folder);
			return;
		}
		begin_incoming_folder(connection, folder);
		auto &stored = incoming_folders_[offer.folder_id] = std::move(folder);
		if (stored.file_count == 0) {
			complete_incoming_folder(connection, stored);
		}
	}

	void prepare_incoming_folder_dest(IncomingFolder &folder) {
		if (folder.dest_root.empty()) {
			folder.dest_root = unique_directory(config_.incoming_dir, folder.root);
		}
		std::filesystem::create_directories(folder.dest_root);
		for (const auto &dir : folder.empty_dirs) {
			if (const auto path = resolve_under_root(folder.dest_root, dir)) {
				std::filesystem::create_directories(*path);
			}
		}
	}

	void begin_incoming_folder(Connection &connection, IncomingFolder &folder) {
		folder.accepted = true;
		prepare_incoming_folder_dest(folder);
		file_events_.push_back(make_folder_event(
			folder.peer_id,
			folder.folder_id,
			folder.group_id,
			folder.root,
			folder.dest_root,
			folder.file_count,
			0,
			false,
			FileEvent::Kind::Started));
		queue_frame(connection, FolderAck{folder.folder_id, folder.files_done});
	}

	void on_folder_ack(const FolderAck &ack) {
		auto *folder = find_outgoing_folder(ack.folder_id);
		if (!folder || folder->finished) {
			return;
		}
		const bool first = !folder->offer_acked;
		folder->offer_acked = true;
		release_folder_files(*folder);
		if (first) {
			file_events_.push_back(make_folder_event(
				folder->peer_id,
				folder->folder_id,
				folder->group_id,
				folder->root,
				folder->source,
				folder->file_count,
				folder->files_done,
				true,
				FileEvent::Kind::Started));
		}
	}

	void on_folder_complete(const FolderComplete &done) {
		auto *folder = find_outgoing_folder(done.folder_id);
		if (!folder || folder->finished) {
			return;
		}
		if (folder->files_done < folder->file_count) {
			return;
		}
		folder->finished = true;
		file_events_.push_back(make_folder_event(
			folder->peer_id,
			folder->folder_id,
			folder->group_id,
			folder->root,
			folder->source,
			folder->file_count,
			folder->files_done,
			true,
			FileEvent::Kind::Completed));
	}

	void on_folder_abort(const FolderAbort &abort) {
		if (auto *folder = find_outgoing_folder(abort.folder_id)) {
			const bool declined = !folder->offer_acked
				|| abort.reason == kAbortRejected;
			fail_outgoing_folder(*folder, declined ? "rejected" : "peer aborted the folder");
		}
		auto it = incoming_folders_.find(abort.folder_id);
		if (it != incoming_folders_.end() && !it->second.finished) {
			it->second.finished = true;
			file_events_.push_back(make_folder_event(
				it->second.peer_id,
				it->second.folder_id,
				it->second.group_id,
				it->second.root,
				it->second.dest_root,
				it->second.file_count,
				it->second.files_done,
				false,
				FileEvent::Kind::Failed,
				"peer aborted the folder"));
		}
	}

	void fail_incoming_folder(
			IncomingFolder &folder,
			const std::string &error,
			uint8_t reason = 1) {
		if (folder.finished) {
			return;
		}
		folder.finished = true;
		for (auto &entry : incoming_files_) {
			if (entry.second.folder_id == folder.folder_id && !entry.second.finished) {
				fail_incoming(entry.second, error, reason);
			}
		}
		if (auto *connection = find_connection(folder.peer_id)) {
			queue_frame(*connection, FolderAbort{folder.folder_id, reason});
		}
		file_events_.push_back(make_folder_event(
			folder.peer_id,
			folder.folder_id,
			folder.group_id,
			folder.root,
			folder.dest_root,
			folder.file_count,
			folder.files_done,
			false,
			FileEvent::Kind::Failed,
			error));
	}

	std::filesystem::path resolve_accept_file_path(
			const IncomingFile &file,
			const std::filesystem::path &dest) const {
		if (dest.empty()) {
			const auto found = incoming_dest_for(file.folder_id, file.filename);
			return found ? *found : unique_destination(config_.incoming_dir, file.filename);
		}
		std::error_code ec;
		if (std::filesystem::is_directory(dest, ec)) {
			return unique_destination(dest, file.filename);
		}
		auto parent = dest.parent_path();
		if (parent.empty()) {
			parent = config_.incoming_dir;
		}
		if (std::filesystem::exists(dest, ec)) {
			return unique_destination(parent, dest.filename().string());
		}
		return dest;
	}

	bool accept_transfer(const std::string &id, const std::filesystem::path &dest) {
		if (id.empty()) {
			return false;
		}
		auto incoming = incoming_files_.find(id);
		if (incoming != incoming_files_.end()
			&& !incoming->second.finished
			&& !incoming->second.accepted) {
			incoming->second.dest_path = resolve_accept_file_path(incoming->second, dest);
			auto *connection = find_connection(incoming->second.peer_id);
			if (!connection) {
				incoming->second.temp_path = incoming->second.dest_path;
				incoming->second.temp_path += ".part";
				incoming->second.accepted = true;
				if (!open_incoming_part(
						incoming->second,
						std::filesystem::exists(incoming->second.temp_path))) {
					incoming->second.accepted = false;
					return false;
				}
				file_events_.push_back(make_event(incoming->second, FileEvent::Kind::Started));
				persist(incoming->second);
				return true;
			}
			if (!begin_incoming_file(*connection, incoming->second)) {
				return false;
			}
			persist(incoming->second);
			queue_frame(*connection, FileAck{incoming->second.transfer_id, incoming->second.acked_count});
			return true;
		}
		auto folder = incoming_folders_.find(id);
		if (folder != incoming_folders_.end()
			&& !folder->second.finished
			&& !folder->second.accepted) {
			if (dest.empty()) {
				folder->second.dest_root = unique_directory(config_.incoming_dir, folder->second.root);
			} else {
				std::error_code ec;
				if (std::filesystem::is_directory(dest, ec)
					&& !std::filesystem::is_empty(dest, ec)) {
					folder->second.dest_root = unique_directory(dest, folder->second.root);
				} else {
					folder->second.dest_root = dest;
				}
			}
			auto *connection = find_connection(folder->second.peer_id);
			if (!connection) {
				folder->second.accepted = true;
				prepare_incoming_folder_dest(folder->second);
				file_events_.push_back(make_folder_event(
					folder->second.peer_id,
					folder->second.folder_id,
					folder->second.group_id,
					folder->second.root,
					folder->second.dest_root,
					folder->second.file_count,
					0,
					false,
					FileEvent::Kind::Started));
				return true;
			}
			begin_incoming_folder(*connection, folder->second);
			if (folder->second.file_count == 0) {
				complete_incoming_folder(*connection, folder->second);
			}
			return true;
		}
		return false;
	}

	bool reject_transfer(const std::string &id) {
		if (id.empty()) {
			return false;
		}
		auto incoming = incoming_files_.find(id);
		if (incoming != incoming_files_.end() && !incoming->second.finished) {
			if (incoming->second.accepted) {
				fail_incoming(incoming->second, "rejected", kAbortRejected);
			} else {
				incoming->second.finished = true;
				file_events_.push_back(make_event(incoming->second, FileEvent::Kind::Failed, "rejected"));
				if (auto *connection = find_connection(incoming->second.peer_id)) {
					queue_frame(*connection, FileAbort{incoming->second.transfer_id, kAbortRejected});
				}
			}
			return true;
		}
		auto folder = incoming_folders_.find(id);
		if (folder != incoming_folders_.end() && !folder->second.finished) {
			if (folder->second.accepted) {
				fail_incoming_folder(folder->second, "rejected", kAbortRejected);
			} else {
				folder->second.finished = true;
				file_events_.push_back(make_folder_event(
					folder->second.peer_id,
					folder->second.folder_id,
					folder->second.group_id,
					folder->second.root,
					folder->second.dest_root,
					folder->second.file_count,
					folder->second.files_done,
					false,
					FileEvent::Kind::Failed,
					"rejected"));
				if (auto *connection = find_connection(folder->second.peer_id)) {
					queue_frame(*connection, FolderAbort{folder->second.folder_id, kAbortRejected});
				}
			}
			return true;
		}
		return false;
	}

	void set_incoming_dir(const std::filesystem::path &dir) {
		if (!dir.empty()) {
			config_.incoming_dir = dir;
			std::filesystem::create_directories(config_.incoming_dir);
		}
	}

	void test_force_outgoing_digest(const Sha256 &digest) {
		forced_digest_ = digest;
	}

	bool abort_transfer(const std::string &id) {
		if (id.empty()) {
			return false;
		}
		bool any = false;
		if (auto *file = find_outgoing(id)) {
			if (!file->finished) {
				fail_outgoing(*file, "cancelled");
				any = true;
			}
		}
		auto incoming = incoming_files_.find(id);
		if (incoming != incoming_files_.end() && !incoming->second.finished) {
			fail_incoming(incoming->second, "cancelled", 1);
			any = true;
		}
		if (auto *folder = find_outgoing_folder(id)) {
			if (!folder->finished) {
				fail_outgoing_folder(*folder, "cancelled");
				any = true;
			}
		}
		auto incoming_folder = incoming_folders_.find(id);
		if (incoming_folder != incoming_folders_.end() && !incoming_folder->second.finished) {
			fail_incoming_folder(incoming_folder->second, "cancelled");
			any = true;
		}
		for (auto &file : outgoing_files_) {
			if (!file.finished && file.batch_id == id) {
				fail_outgoing(file, "cancelled");
				any = true;
			}
		}
		for (auto &folder : outgoing_folders_) {
			if (!folder.finished && folder.batch_id == id) {
				fail_outgoing_folder(folder, "cancelled");
				any = true;
			}
		}
		return any;
	}

	uint64_t now_system_ms() const {
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch())
				.count());
	}

	void queue_recall(const std::string &peer_id, const Recall &recall) {
		if (auto *connection = find_connection(peer_id)) {
			queue_frame(*connection, recall);
			return;
		}
		pending_recalls_[peer_id].push_back(recall);
		try_connect_peer(peer_id);
	}

	bool recall_message(
			const std::string &chat_id,
			const std::string &msg_id,
			bool is_group,
			uint64_t sent_ms) {
		if (!is_valid_peer_id(msg_id) || !within_recall_window(sent_ms, now_system_ms())) {
			return false;
		}
		Recall recall;
		recall.msg_id = msg_id;
		if (is_group) {
			if (!is_valid_peer_id(chat_id)) {
				return false;
			}
			auto it = groups_.find(chat_id);
			if (it == groups_.end()) {
				return false;
			}
			recall.group_id = chat_id;
			for (const auto &member : it->second.member_ids) {
				if (member != config_.identity.peer_id) {
					queue_recall(member, recall);
				}
			}
			return true;
		}
		if (!is_valid_peer_id(chat_id)) {
			return false;
		}
		queue_recall(chat_id, recall);
		return true;
	}

	bool set_display_name(std::string_view name) {
		const auto normalized = normalize_display_name(name);
		if (normalized.empty()) {
			return false;
		}
		config_.identity.display_name = normalized;
		last_announce_ = Clock::time_point{};
		if (started_ && tcp_port_ != 0) {
			announce_if_due();
		}
		return true;
	}

	void set_profile(std::string_view first, std::string_view last, std::string_view bio) {
		auto clip = [](std::string_view text, size_t max) {
			std::string out;
			for (unsigned char ch : text) {
				if (ch < 0x20 && ch != '\t') {
					continue;
				}
				out.push_back(static_cast<char>(ch));
			}
			if (out.size() > max) {
				out.resize(max);
			}
			return out;
		};
		first_name_ = clip(first, 64);
		last_name_ = clip(last, 64);
		bio_ = clip(bio, 140);
		last_announce_ = Clock::time_point{};
		if (started_ && tcp_port_ != 0) {
			announce_if_due();
		}
	}

	void set_encrypt_files(bool enabled) {
		config_.encrypt_files = enabled;
	}

	void on_recall(Connection &connection, const Recall &recall) {
		IncomingRecall event;
		event.from_peer_id = connection.peer_id;
		event.msg_id = recall.msg_id;
		event.group_id = recall.group_id;
		recalls_.push_back(std::move(event));
	}

	bool known_peer(const std::string &peer_id) {
		return seen_.find(peer_id) != seen_.end() || find_connection(peer_id) != nullptr;
	}

	void persist_group(const Group &group) {
		save_group(groups_dir(), group);
	}

	void emit_group(GroupEvent::Kind kind, const Group &group) {
		GroupEvent event;
		event.kind = kind;
		event.group = group;
		group_events_.push_back(std::move(event));
	}

	bool queue_group_frame(
			const std::string &peer_id,
			const Message &message) {
		auto *connection = find_connection(peer_id);
		const auto *key = lookup_public_key(peer_id, connection);
		if (!connection || !key) {
			try_connect_peer(peer_id);
			return false;
		}
		queue_frame(*connection, message);
		return true;
	}

	void send_group_roster(
			const Group &group,
			const std::string &peer_id,
			bool invite) {
		const auto plain = encode_group_roster(roster_from_group(group));
		const auto sealed = seal_for(
			peer_id,
			find_connection(peer_id),
			std::string_view(
				reinterpret_cast<const char *>(plain.data()),
				plain.size()));
		if (invite) {
			queue_group_frame(peer_id, GroupInvite{group.group_id, sealed});
		} else {
			queue_group_frame(peer_id, GroupUpdate{group.group_id, sealed});
		}
	}

	void fanout_roster(const Group &group, const std::string *invitee) {
		for (const auto &member : group.member_ids) {
			if (member == config_.identity.peer_id) {
				continue;
			}
			try {
				send_group_roster(group, member, invitee && member == *invitee);
			} catch (const std::exception &) {
				// Fail closed for that member; other members still receive.
			}
		}
	}

	void on_group_roster(
			Connection &connection,
			const std::string &group_id,
			const std::vector<uint8_t> &sealed) {
		const auto opened = open_from(connection.peer_id, &connection, sealed);
		if (!opened) {
			return;
		}
		const auto roster = decode_group_roster(
			reinterpret_cast<const uint8_t *>(opened->data()),
			opened->size());
		if (!roster) {
			return;
		}
		auto group = group_from_roster(group_id, *roster);
		if (!group_has_member(group, config_.identity.peer_id)) {
			groups_.erase(group_id);
			remove_group(groups_dir(), group_id);
			emit_group(GroupEvent::Kind::Removed, group);
			return;
		}
		groups_[group_id] = group;
		persist_group(group);
		emit_group(GroupEvent::Kind::Upserted, group);
	}

	void on_group_text(Connection &connection, const GroupText &text) {
		auto it = groups_.find(text.group_id);
		if (it == groups_.end() || !group_has_member(it->second, connection.peer_id)) {
			return;
		}
		const auto opened = open_from(connection.peer_id, &connection, text.sealed);
		if (!opened) {
			return;
		}
		IncomingText incoming;
		incoming.from_peer_id = connection.peer_id;
		incoming.from_name = connection.display_name;
		incoming.msg_id = text.msg_id;
		incoming.timestamp_ms = text.timestamp_ms;
		incoming.body = *opened;
		incoming.group_id = text.group_id;
		texts_.push_back(std::move(incoming));
		queue_frame(connection, Ack{text.msg_id});
	}

	std::string send_folder(
			const std::string &peer_id,
			const std::filesystem::path &dir,
			const std::string &group_id = {},
			const std::string &batch_id = {}) {
		if (peer_id == config_.identity.peer_id) {
			throw std::invalid_argument("cannot send to self");
		}
		if (!known_peer(peer_id)) {
			throw std::runtime_error("unknown peer");
		}
		const auto tree = walk_folder(dir);
		if (!tree) {
			throw std::runtime_error("unable to read folder");
		}
		OutgoingFolder folder;
		folder.folder_id = random_hex_id();
		folder.peer_id = peer_id;
		folder.group_id = group_id;
		folder.batch_id = batch_id;
		folder.root = tree->root;
		folder.source = dir;
		folder.empty_dirs = tree->empty_dirs;
		folder.file_count = static_cast<uint32_t>(tree->files.size());
		for (const auto &entry : tree->files) {
			const auto id = enqueue_file(
				peer_id,
				entry.source,
				entry.relative,
				{},
				folder.folder_id,
				true,
				group_id,
				batch_id);
			folder.file_ids.push_back(id);
		}
		const auto id = folder.folder_id;
		outgoing_folders_.push_back(std::move(folder));
		try {
			if (find_connection(peer_id)) {
				send_folder_offer(outgoing_folders_.back());
			} else {
				try_connect_peer(peer_id);
			}
		} catch (const std::exception &error) {
			fail_outgoing_folder(outgoing_folders_.back(), error.what());
		}
		return id;
	}

	std::string enqueue_file(
			const std::string &peer_id,
			const std::filesystem::path &path,
			std::string_view display_name,
			std::string_view mime,
			const std::string &folder_id,
			bool held,
			const std::string &group_id = {},
			const std::string &batch_id = {}) {
		std::error_code ec;
		if (!std::filesystem::is_regular_file(path, ec)) {
			throw std::runtime_error("not a regular file");
		}
		const auto size = std::filesystem::file_size(path, ec);
		if (ec) {
			throw std::runtime_error("unable to read file size");
		}
		if (size > kMaxFileSize) {
			throw std::invalid_argument("file is too large");
		}
		OutgoingFile file;
		file.transfer_id = random_hex_id();
		file.peer_id = peer_id;
		file.folder_id = folder_id;
		file.group_id = group_id;
		file.batch_id = batch_id;
		file.path = path;
		if (folder_id.empty()) {
			file.filename = sanitize_filename(
				display_name.empty() ? path.filename().string() : std::string(display_name));
		} else {
			const auto relative = sanitize_relative_path(
				display_name.empty() ? path.filename().string() : std::string(display_name));
			if (!relative) {
				throw std::invalid_argument("unsafe relative path");
			}
			file.filename = *relative;
		}
		file.mime = std::string(mime);
		file.size = size;
		file.encrypted = config_.encrypt_files;
		// LocalSend-style: start the plaintext stream immediately. The
		// receiver skips the post-hash when the offer digest is zero.
		// Encrypted transfers still hash first so resume can match.
		file.sha256 = file.encrypted ? sha256_file(path) : Sha256{};
		file.chunk_size = file.encrypted ? kFileChunkPlainMax : kFileChunkPlainFast;
		file.chunk_count = file_chunk_count(size, file.chunk_size);
		file.held = held;
		const auto id = file.transfer_id;
		outgoing_files_.push_back(std::move(file));
		persist(outgoing_files_.back());
		if (!held) {
			try {
				if (find_connection(peer_id)) {
					send_offer(outgoing_files_.back());
				} else {
					try_connect_peer(peer_id);
				}
			} catch (const std::exception &error) {
				fail_outgoing(outgoing_files_.back(), error.what());
			}
		}
		return id;
	}

	std::string send_file(
			const std::string &peer_id,
			const std::filesystem::path &path,
			std::string_view display_name,
			std::string_view mime) {
		if (peer_id == config_.identity.peer_id) {
			throw std::invalid_argument("cannot send to self");
		}
		if (!known_peer(peer_id)) {
			throw std::runtime_error("unknown peer");
		}
		return enqueue_file(peer_id, path, display_name, mime, {}, false);
	}

	std::string create_group(
			std::string_view name,
			const std::vector<std::string> &member_ids) {
		for (const auto &member : member_ids) {
			if (member != config_.identity.peer_id && !known_peer(member)) {
				throw std::runtime_error("unknown peer");
			}
		}
		auto group = make_group(name, config_.identity.peer_id, member_ids);
		groups_[group.group_id] = group;
		persist_group(group);
		emit_group(GroupEvent::Kind::Upserted, group);
		fanout_roster(group, nullptr);
		return group.group_id;
	}

	bool add_group_member(const std::string &group_id, const std::string &peer_id) {
		auto it = groups_.find(group_id);
		if (it == groups_.end() || !known_peer(peer_id)) {
			return false;
		}
		if (!group_add_member(it->second, peer_id)) {
			return false;
		}
		persist_group(it->second);
		emit_group(GroupEvent::Kind::Upserted, it->second);
		fanout_roster(it->second, &peer_id);
		return true;
	}

	bool remove_group_member(const std::string &group_id, const std::string &peer_id) {
		auto it = groups_.find(group_id);
		if (it == groups_.end()) {
			return false;
		}
		if (!group_remove_member(it->second, peer_id)) {
			return false;
		}
		persist_group(it->second);
		emit_group(GroupEvent::Kind::Upserted, it->second);
		try {
			send_group_roster(it->second, peer_id, false);
		} catch (const std::exception &) {
		}
		fanout_roster(it->second, nullptr);
		return true;
	}

	std::string send_group_text(const std::string &group_id, std::string_view body) {
		auto it = groups_.find(group_id);
		if (it == groups_.end() || !group_has_member(it->second, config_.identity.peer_id)) {
			throw std::runtime_error("unknown group");
		}
		if (body.size() > kMaxTextPlain) {
			throw std::invalid_argument("text too large");
		}
		const auto msg_id = random_hex_id();
		const auto timestamp = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch())
				.count());
		for (const auto &member : it->second.member_ids) {
			if (member == config_.identity.peer_id) {
				continue;
			}
			try {
				auto *connection = find_connection(member);
				const auto sealed = seal_for(member, connection, body);
				GroupText text;
				text.group_id = group_id;
				text.msg_id = msg_id;
				text.timestamp_ms = timestamp;
				text.sealed = sealed;
				if (!queue_group_frame(member, text)) {
					PendingText pending;
					pending.msg_id = msg_id;
					pending.timestamp_ms = timestamp;
					pending.body = std::string(body);
					pending.group_id = group_id;
					queued_[member].push_back(std::move(pending));
					try_connect_peer(member);
				}
			} catch (const std::exception &) {
				// Fail closed for that member.
			}
		}
		return msg_id;
	}

	std::string send_group_file(
			const std::string &group_id,
			const std::filesystem::path &path,
			std::string_view display_name,
			std::string_view mime) {
		auto it = groups_.find(group_id);
		if (it == groups_.end() || !group_has_member(it->second, config_.identity.peer_id)) {
			throw std::runtime_error("unknown group");
		}
		const auto batch_id = random_hex_id();
		for (const auto &member : it->second.member_ids) {
			if (member == config_.identity.peer_id) {
				continue;
			}
			try {
				enqueue_file(member, path, display_name, mime, {}, false, group_id, batch_id);
			} catch (const std::exception &) {
				// Fail closed for that member.
			}
		}
		return batch_id;
	}

	std::string send_group_folder(
			const std::string &group_id,
			const std::filesystem::path &dir) {
		auto it = groups_.find(group_id);
		if (it == groups_.end() || !group_has_member(it->second, config_.identity.peer_id)) {
			throw std::runtime_error("unknown group");
		}
		const auto batch_id = random_hex_id();
		for (const auto &member : it->second.member_ids) {
			if (member == config_.identity.peer_id) {
				continue;
			}
			try {
				send_folder(member, dir, group_id, batch_id);
			} catch (const std::exception &) {
				// Fail closed for that member.
			}
		}
		return batch_id;
	}

	std::string send_text(const std::string &peer_id, std::string_view body) {
		if (peer_id == config_.identity.peer_id) {
			throw std::invalid_argument("cannot send to self");
		}
		if (seen_.find(peer_id) == seen_.end() && !find_connection(peer_id)) {
			throw std::runtime_error("unknown peer");
		}
		if (body.size() > kMaxTextPlain) {
			throw std::invalid_argument("text too large");
		}
		PendingText pending;
		pending.msg_id = random_hex_id();
		pending.timestamp_ms = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch())
				.count());
		pending.body = std::string(body);
		const auto msg_id = pending.msg_id;
		if (!flush_pending(peer_id, pending)) {
			queued_[peer_id].push_back(std::move(pending));
			try_connect_peer(peer_id);
		}
		return msg_id;
	}

	NodeConfig config_;
	std::string first_name_;
	std::string last_name_;
	std::string bio_;
	Socket udp_fd_ = kInvalidSocket;
	Socket listen_fd_ = kInvalidSocket;
	uint16_t udp_port_ = 0;
	uint16_t tcp_port_ = 0;
	bool started_ = false;
	Clock::time_point last_announce_{};
	std::vector<sockaddr_in> targets_;
	std::map<std::string, Seen> seen_;
	std::vector<Connection> connections_;
	std::map<std::string, std::vector<PendingText>> queued_;
	std::vector<PeerInfo> peer_events_;
	std::vector<IncomingText> texts_;
	std::vector<std::string> acks_;
	std::vector<OutgoingFile> outgoing_files_;
	std::map<std::string, IncomingFile> incoming_files_;
	std::vector<OutgoingFolder> outgoing_folders_;
	std::map<std::string, IncomingFolder> incoming_folders_;
	std::map<std::string, Group> groups_;
	std::vector<FileEvent> file_events_;
	std::optional<Sha256> forced_digest_;
	std::vector<GroupEvent> group_events_;
	std::vector<IncomingRecall> recalls_;
	std::map<std::string, std::vector<Recall>> pending_recalls_;

	Connection *find_connection(const std::string &peer_id) {
		for (auto &connection : connections_) {
			if (connection.hello_received && connection.peer_id == peer_id) {
				return &connection;
			}
		}
		return nullptr;
	}

	void queue_frame(Connection &connection, const Message &message) {
		const auto frame = encode_frame(message);
		connection.write_buf.insert(
			connection.write_buf.end(),
			frame.begin(),
			frame.end());
	}

	bool flush_pending(const std::string &peer_id, PendingText &pending) {
		auto *connection = find_connection(peer_id);
		if (!connection) {
			return false;
		}
		const auto *key = lookup_public_key(peer_id, connection);
		if (!key) {
			// Fail closed: never put plaintext on the wire.
			return false;
		}
		const auto cipher = config_.e2e_key->encrypt_for(*key, pending.body);
		if (!pending.group_id.empty()) {
			GroupText text;
			text.group_id = pending.group_id;
			text.msg_id = pending.msg_id;
			text.timestamp_ms = pending.timestamp_ms;
			text.sealed = cipher;
			queue_frame(*connection, text);
		} else {
			std::string body(cipher.begin(), cipher.end());
			queue_frame(*connection, Text{pending.msg_id, pending.timestamp_ms, std::move(body)});
		}
		return true;
	}

	void flush_group_roster(const std::string &peer_id) {
		for (const auto &entry : groups_) {
			if (!group_has_member(entry.second, peer_id)) {
				continue;
			}
			try {
				send_group_roster(entry.second, peer_id, true);
			} catch (const std::exception &) {
			}
		}
	}

	void flush_pending_recalls(const std::string &peer_id) {
		auto pending = pending_recalls_.find(peer_id);
		if (pending == pending_recalls_.end()) {
			return;
		}
		if (auto *connection = find_connection(peer_id)) {
			for (const auto &recall : pending->second) {
				queue_frame(*connection, recall);
			}
			pending_recalls_.erase(pending);
		}
	}

	void flush_queue(const std::string &peer_id) {
		flush_group_roster(peer_id);
		flush_pending_recalls(peer_id);
		auto queued = queued_.find(peer_id);
		if (queued == queued_.end()) {
			return;
		}
		for (auto &pending : queued->second) {
			flush_pending(peer_id, pending);
		}
		queued_.erase(queued);
	}

	void announce_if_due() {
		const auto now = Clock::now();
		if (last_announce_ != Clock::time_point{}
			&& now - last_announce_ < config_.announce_interval) {
			return;
		}
		last_announce_ = now;
		Announce announce;
		announce.peer_id = config_.identity.peer_id;
		announce.display_name = config_.identity.display_name;
		announce.tcp_port = tcp_port_;
		announce.public_key = config_.e2e_key->public_key();
		announce.first_name = first_name_;
		announce.last_name = last_name_;
		announce.bio = bio_;
		const auto packet = encode_announce(announce);
		auto send_to = [&](const sockaddr_in &addr) {
			socket_sendto(
				udp_fd_,
				packet.data(),
				packet.size(),
				reinterpret_cast<const sockaddr *>(&addr),
				sizeof(addr));
		};
		if (!config_.multicast_group.empty()) {
			send_to(make_addr(config_.multicast_group, config_.discovery_port));
		}
		for (const auto &target : targets_) {
			send_to(target);
		}
	}

	void expire_peers() {
		const auto now = Clock::now();
		for (auto it = seen_.begin(); it != seen_.end();) {
			if (now - it->second.last_seen > config_.peer_timeout) {
				it = seen_.erase(it);
			} else {
				++it;
			}
		}
	}

	void remember_peer(const PeerInfo &info) {
		if (info.peer_id == config_.identity.peer_id) {
			return;
		}
		auto &seen = seen_[info.peer_id];
		const bool is_new = seen.info.peer_id.empty();
		const auto known_port = seen.info.tcp_port;
		const auto known_key = seen.info.public_key;
		seen.info = info;
		if (seen.info.tcp_port == 0 && known_port != 0) {
			seen.info.tcp_port = known_port;
		}
		if (!is_valid_public_key(seen.info.public_key)
			&& is_valid_public_key(known_key)) {
			seen.info.public_key = known_key;
		}
		seen.last_seen = Clock::now();
		if (is_new) {
			peer_events_.push_back(seen.info);
		}
	}

	void recv_udp() {
		for (;;) {
			uint8_t buffer[2048];
			sockaddr_in from{};
			socklen_t from_len = sizeof(from);
			const auto n = socket_recvfrom(
				udp_fd_,
				buffer,
				sizeof(buffer),
				reinterpret_cast<sockaddr *>(&from),
				&from_len);
			if (n < 0) {
				if (error_would_block()) {
					return;
				}
				throw_socket_error("recvfrom");
			}
			const auto announce = decode_announce(buffer, static_cast<size_t>(n));
			if (!announce) {
				continue;
			}
			PeerInfo info;
			info.peer_id = announce->peer_id;
			info.display_name = announce->display_name;
			info.ip = addr_ip(from);
			info.tcp_port = announce->tcp_port;
			info.public_key = announce->public_key;
			info.first_name = announce->first_name;
			info.last_name = announce->last_name;
			info.bio = announce->bio;
			remember_peer(info);
		}
	}

	void accept_tcp() {
		for (;;) {
			sockaddr_in from{};
			socklen_t from_len = sizeof(from);
			const Socket fd = ::accept(
				listen_fd_,
				reinterpret_cast<sockaddr *>(&from),
				&from_len);
			if (!valid_socket(fd)) {
				if (error_would_block()) {
					return;
				}
				throw_socket_error("accept");
			}
			set_nonblock(fd);
			configure_tcp_socket(fd);
			Connection connection;
			connection.fd = fd;
			connection.ip = addr_ip(from);
			queue_frame(connection, make_hello());
			connection.hello_sent = true;
			connections_.push_back(std::move(connection));
		}
	}

	bool has_work_for(const std::string &peer_id) const {
		const auto queued = queued_.find(peer_id);
		if (queued != queued_.end() && !queued->second.empty()) {
			return true;
		}
		for (const auto &file : outgoing_files_) {
			if (file.peer_id == peer_id && !file.finished) {
				return true;
			}
		}
		for (const auto &folder : outgoing_folders_) {
			if (folder.peer_id == peer_id && !folder.finished) {
				return true;
			}
		}
		for (const auto &entry : groups_) {
			if (group_has_member(entry.second, peer_id)) {
				return true;
			}
		}
		return false;
	}

	void try_connect_known_peers() {
		for (const auto &entry : seen_) {
			try_connect_peer(entry.first);
		}
	}

	void try_connect_peer(const std::string &peer_id) {
		if (find_connection(peer_id)) {
			return;
		}
		for (const auto &connection : connections_) {
			if (connection.peer_id == peer_id ||
				(connection.ip.size() && seen_.count(peer_id)
					&& connection.ip == seen_.at(peer_id).info.ip
					&& connection.tcp_port == seen_.at(peer_id).info.tcp_port)) {
				return;
			}
		}
		const auto seen = seen_.find(peer_id);
		if (seen == seen_.end()) {
			return;
		}
		// Lower peer_id initiates, unless we have queued messages to flush.
		const bool must_send = has_work_for(peer_id);
		if (!must_send && config_.identity.peer_id > peer_id) {
			return;
		}
		const Socket fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (!valid_socket(fd)) {
			return;
		}
		set_nonblock(fd);
		configure_tcp_socket(fd);
		auto addr = make_addr(seen->second.info.ip, seen->second.info.tcp_port);
		const int rc = ::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
		if (rc < 0 && !error_connect_pending()) {
			Socket doomed = fd;
			close_socket(doomed);
			return;
		}
		Connection connection;
		connection.fd = fd;
		connection.connecting = (rc < 0);
		connection.ip = seen->second.info.ip;
		connection.tcp_port = seen->second.info.tcp_port;
		connection.peer_id = peer_id;
		if (!connection.connecting) {
			queue_frame(connection, make_hello());
			connection.hello_sent = true;
		}
		connections_.push_back(std::move(connection));
	}

	void handle_connection(Connection &connection, short revents) {
		if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
			if (connection.connecting) {
				close_connection(connection);
				return;
			}
		}
		if ((revents & POLLOUT) && connection.connecting) {
			int err = 0;
			socklen_t len = sizeof(err);
			if (get_sockopt(connection.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
				close_connection(connection);
				return;
			}
			connection.connecting = false;
			if (!connection.hello_sent) {
				queue_frame(connection, make_hello());
				connection.hello_sent = true;
			}
		}
		if ((revents & POLLOUT) && !connection.connecting && write_pending(connection)) {
			const auto n = socket_send(
				connection.fd,
				connection.write_buf.data() + connection.write_off,
				connection.write_buf.size() - connection.write_off);
			if (n < 0) {
				if (!error_would_block()) {
					close_connection(connection);
				}
				return;
			}
			connection.write_off += static_cast<size_t>(n);
			compact_write(connection);
		}
		if (revents & POLLIN) {
			uint8_t buffer[64 * 1024];
			const auto n = socket_recv(connection.fd, buffer, sizeof(buffer));
			if (n <= 0) {
				if (n < 0 && error_would_block()) {
					return;
				}
				close_connection(connection);
				return;
			}
			connection.read_buf.insert(
				connection.read_buf.end(),
				buffer,
				buffer + n);
			for (;;) {
				const auto decoded = try_decode_frame(
					connection.read_buf.data(),
					connection.read_buf.size());
				if (!decoded) {
					break;
				}
				connection.read_buf.erase(
					connection.read_buf.begin(),
					connection.read_buf.begin() + static_cast<std::ptrdiff_t>(decoded->consumed));
				on_message(connection, decoded->message);
				if (!valid_socket(connection.fd)) {
					return;
				}
			}
		}
	}

	void on_message(Connection &connection, const Message &message) {
		if (const auto *hello = std::get_if<Hello>(&message)) {
			if (hello->peer_id == config_.identity.peer_id) {
				close_connection(connection);
				return;
			}
			if (auto *existing = find_connection(hello->peer_id);
				existing && existing != &connection) {
				close_connection(connection);
				return;
			}
			connection.peer_id = hello->peer_id;
			connection.display_name = hello->display_name;
			connection.public_key = hello->public_key;
			connection.hello_received = true;
			PeerInfo info;
			info.peer_id = hello->peer_id;
			info.display_name = hello->display_name;
			info.ip = connection.ip;
			info.tcp_port = connection.tcp_port;
			info.public_key = hello->public_key;
			info.first_name = hello->first_name;
			info.last_name = hello->last_name;
			info.bio = hello->bio;
			remember_peer(info);
			flush_queue(hello->peer_id);
			return;
		}
		if (!connection.hello_received) {
			close_connection(connection);
			return;
		}
		if (const auto *text = std::get_if<Text>(&message)) {
			const auto *key = lookup_public_key(connection.peer_id, &connection);
			if (!key) {
				return;
			}
			const auto opened = config_.e2e_key->decrypt_from(
				*key,
				std::string_view(text->body));
			if (!opened) {
				return;
			}
			IncomingText incoming;
			incoming.from_peer_id = connection.peer_id;
			incoming.from_name = connection.display_name;
			incoming.msg_id = text->msg_id;
			incoming.timestamp_ms = text->timestamp_ms;
			incoming.body = *opened;
			texts_.push_back(std::move(incoming));
			queue_frame(connection, Ack{text->msg_id});
			return;
		}
		if (const auto *ack = std::get_if<Ack>(&message)) {
			acks_.push_back(ack->msg_id);
			return;
		}
		if (const auto *offer = std::get_if<FileOffer>(&message)) {
			on_file_offer(connection, *offer);
			return;
		}
		if (const auto *chunk = std::get_if<FileChunk>(&message)) {
			on_file_chunk(connection, *chunk);
			return;
		}
		if (const auto *file_ack = std::get_if<FileAck>(&message)) {
			on_file_ack(*file_ack);
			return;
		}
		if (const auto *done = std::get_if<FileComplete>(&message)) {
			on_file_complete(*done);
			return;
		}
		if (const auto *digest = std::get_if<FileDigest>(&message)) {
			on_file_digest(connection, *digest);
			return;
		}
		if (const auto *abort = std::get_if<FileAbort>(&message)) {
			on_file_abort(connection, *abort);
			return;
		}
		if (const auto *folder = std::get_if<FolderOffer>(&message)) {
			on_folder_offer(connection, *folder);
			return;
		}
		if (const auto *ack = std::get_if<FolderAck>(&message)) {
			on_folder_ack(*ack);
			return;
		}
		if (const auto *done = std::get_if<FolderComplete>(&message)) {
			on_folder_complete(*done);
			return;
		}
		if (const auto *folder_abort = std::get_if<FolderAbort>(&message)) {
			on_folder_abort(*folder_abort);
			return;
		}
		if (const auto *invite = std::get_if<GroupInvite>(&message)) {
			on_group_roster(connection, invite->group_id, invite->sealed);
			return;
		}
		if (const auto *update = std::get_if<GroupUpdate>(&message)) {
			on_group_roster(connection, update->group_id, update->sealed);
			return;
		}
		if (const auto *group_text = std::get_if<GroupText>(&message)) {
			on_group_text(connection, *group_text);
			return;
		}
		if (const auto *recall = std::get_if<Recall>(&message)) {
			on_recall(connection, *recall);
			return;
		}
		if (std::holds_alternative<Bye>(message)) {
			close_connection(connection);
		}
	}
};

Node::Node(NodeConfig config)
	: impl_(std::make_unique<Impl>(std::move(config))) {}

Node::~Node() = default;

void Node::start() {
	impl_->start();
}

void Node::stop() {
	impl_->stop();
}

void Node::step(std::chrono::milliseconds timeout) {
	impl_->step(timeout);
}

bool Node::needs_spin() const {
	return impl_->needs_spin();
}

const Identity &Node::identity() const {
	return impl_->config_.identity;
}

const std::vector<uint8_t> &Node::public_key() const {
	return impl_->config_.e2e_key->public_key();
}

uint16_t Node::tcp_port() const {
	return impl_->tcp_port_;
}

uint16_t Node::udp_port() const {
	return impl_->udp_port_;
}

std::vector<PeerInfo> Node::peers() const {
	std::vector<PeerInfo> out;
	for (const auto &entry : impl_->seen_) {
		out.push_back(entry.second.info);
	}
	return out;
}

void Node::add_discovery_target(const std::string &ip, uint16_t port) {
	impl_->add_discovery_target(ip, port);
}

std::string Node::send_text(const std::string &peer_id, std::string_view body) {
	return impl_->send_text(peer_id, body);
}

std::string Node::send_file(
		const std::string &peer_id,
		const std::filesystem::path &path,
		std::string_view display_name,
		std::string_view mime) {
	return impl_->send_file(peer_id, path, display_name, mime);
}

std::string Node::send_folder(
		const std::string &peer_id,
		const std::filesystem::path &dir) {
	return impl_->send_folder(peer_id, dir);
}

std::string Node::create_group(
		std::string_view name,
		const std::vector<std::string> &member_ids) {
	return impl_->create_group(name, member_ids);
}

bool Node::add_group_member(const std::string &group_id, const std::string &peer_id) {
	return impl_->add_group_member(group_id, peer_id);
}

bool Node::remove_group_member(const std::string &group_id, const std::string &peer_id) {
	return impl_->remove_group_member(group_id, peer_id);
}

std::string Node::send_group_text(const std::string &group_id, std::string_view body) {
	return impl_->send_group_text(group_id, body);
}

std::string Node::send_group_file(
		const std::string &group_id,
		const std::filesystem::path &path,
		std::string_view display_name,
		std::string_view mime) {
	return impl_->send_group_file(group_id, path, display_name, mime);
}

std::string Node::send_group_folder(
		const std::string &group_id,
		const std::filesystem::path &dir) {
	return impl_->send_group_folder(group_id, dir);
}

std::vector<Group> Node::groups() const {
	std::vector<Group> out;
	for (const auto &entry : impl_->groups_) {
		out.push_back(entry.second);
	}
	return out;
}

std::vector<PeerInfo> Node::take_peer_events() {
	auto out = std::move(impl_->peer_events_);
	impl_->peer_events_.clear();
	return out;
}

std::vector<IncomingText> Node::take_texts() {
	auto out = std::move(impl_->texts_);
	impl_->texts_.clear();
	return out;
}

std::vector<std::string> Node::take_acks() {
	auto out = std::move(impl_->acks_);
	impl_->acks_.clear();
	return out;
}

std::vector<FileEvent> Node::take_file_events() {
	auto out = std::move(impl_->file_events_);
	impl_->file_events_.clear();
	return out;
}

std::vector<GroupEvent> Node::take_group_events() {
	auto out = std::move(impl_->group_events_);
	impl_->group_events_.clear();
	return out;
}

std::vector<IncomingRecall> Node::take_recalls() {
	auto out = std::move(impl_->recalls_);
	impl_->recalls_.clear();
	return out;
}

bool Node::recall_message(
		const std::string &chat_id,
		const std::string &msg_id,
		bool is_group,
		uint64_t sent_ms) {
	return impl_->recall_message(chat_id, msg_id, is_group, sent_ms);
}

bool Node::accept_transfer(
		const std::string &id,
		const std::filesystem::path &dest) {
	return impl_->accept_transfer(id, dest);
}

bool Node::reject_transfer(const std::string &id) {
	return impl_->reject_transfer(id);
}

void Node::set_incoming_dir(const std::filesystem::path &dir) {
	impl_->set_incoming_dir(dir);
}

std::filesystem::path Node::incoming_dir() const {
	return impl_->config_.incoming_dir;
}

void Node::test_force_outgoing_digest(const std::array<uint8_t, 32> &digest) {
	Sha256 value{};
	std::copy(digest.begin(), digest.end(), value.begin());
	impl_->test_force_outgoing_digest(value);
}

bool Node::abort_transfer(const std::string &id) {
	return impl_->abort_transfer(id);
}

bool Node::set_display_name(std::string_view name) {
	return impl_->set_display_name(name);
}

void Node::set_profile(std::string_view first, std::string_view last, std::string_view bio) {
	impl_->set_profile(first, last, bio);
}

void Node::set_encrypt_files(bool enabled) {
	impl_->set_encrypt_files(enabled);
}

bool Node::encrypt_files() const {
	return impl_->config_.encrypt_files;
}

} // namespace lan
