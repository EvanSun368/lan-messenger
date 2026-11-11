#include "node.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

namespace lan {
namespace {

using Clock = std::chrono::steady_clock;

void throw_errno(const char *what) {
	throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void close_fd(int &fd) {
	if (fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

void set_nonblock(int fd) {
	const int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		throw_errno("fcntl O_NONBLOCK");
	}
}

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

uint16_t bound_port(int fd) {
	sockaddr_in addr{};
	socklen_t len = sizeof(addr);
	if (::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) < 0) {
		throw_errno("getsockname");
	}
	return ntohs(addr.sin_port);
}

int open_udp(const NodeConfig &config, uint16_t &bound) {
	const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		throw_errno("socket udp");
	}
	int yes = 1;
	::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
	::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
	uint16_t port = config.udp_port;
	if (!config.multicast_group.empty()) {
		port = config.discovery_port;
		if (port == 0) {
			::close(fd);
			throw std::invalid_argument("multicast discovery requires a UDP port");
		}
	}
	auto addr = make_addr(config.bind_address, port);
	if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
		::close(fd);
		throw_errno("bind udp");
	}
	if (!config.multicast_group.empty()) {
		u_char loop = 1;
		u_char ttl = 1;
		::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
		::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
		if (config.bind_address != "0.0.0.0") {
			const auto iface = parse_ipv4(config.bind_address);
			::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface));
		}
		ip_mreq membership{};
		membership.imr_multiaddr = parse_ipv4(config.multicast_group);
		membership.imr_interface = parse_ipv4(
			config.bind_address == "0.0.0.0" ? "0.0.0.0" : config.bind_address);
		if (::setsockopt(
				fd,
				IPPROTO_IP,
				IP_ADD_MEMBERSHIP,
				&membership,
				sizeof(membership)) < 0) {
			::close(fd);
			throw_errno("IP_ADD_MEMBERSHIP");
		}
	}
	set_nonblock(fd);
	bound = bound_port(fd);
	return fd;
}

int open_listen(const NodeConfig &config, uint16_t &bound) {
	const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		throw_errno("socket tcp");
	}
	int yes = 1;
	::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	auto addr = make_addr(config.bind_address, config.tcp_port);
	if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
		::close(fd);
		throw_errno("bind tcp");
	}
	if (::listen(fd, 16) < 0) {
		::close(fd);
		throw_errno("listen");
	}
	set_nonblock(fd);
	bound = bound_port(fd);
	return fd;
}

} // namespace

struct Node::Impl {
	struct Connection {
		int fd = -1;
		bool connecting = false;
		bool hello_sent = false;
		bool hello_received = false;
		std::string peer_id;
		std::string display_name;
		std::string ip;
		uint16_t tcp_port = 0;
		std::vector<uint8_t> read_buf;
		std::vector<uint8_t> write_buf;
	};

	struct Seen {
		PeerInfo info;
		Clock::time_point last_seen;
	};

	struct PendingText {
		std::string msg_id;
		uint64_t timestamp_ms = 0;
		std::string body;
	};

	explicit Impl(NodeConfig config)
		: config_(std::move(config)) {
		if (!is_valid_peer_id(config_.identity.peer_id)
			|| !is_valid_display_name(config_.identity.display_name)) {
			throw std::invalid_argument("invalid node identity");
		}
	}

	~Impl() {
		stop();
	}

	void start() {
		if (started_) {
			return;
		}
		udp_fd_ = open_udp(config_, udp_port_);
		listen_fd_ = open_listen(config_, tcp_port_);
		started_ = true;
		last_announce_ = Clock::time_point{};
	}

	void stop() {
		for (auto &connection : connections_) {
			close_fd(connection.fd);
		}
		connections_.clear();
		close_fd(listen_fd_);
		close_fd(udp_fd_);
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
			if (connection.connecting || !connection.write_buf.empty()) {
				events = static_cast<short>(events | POLLOUT);
			}
			fds.push_back({connection.fd, events, 0});
		}
		const int ready = ::poll(fds.data(), fds.size(), static_cast<int>(timeout.count()));
		if (ready < 0) {
			if (errno == EINTR) {
				return;
			}
			throw_errno("poll");
		}
		if (ready == 0) {
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
				[](const Connection &connection) { return connection.fd < 0; }),
			connections_.end());
	}

	std::string send_text(const std::string &peer_id, std::string_view body) {
		if (peer_id == config_.identity.peer_id) {
			throw std::invalid_argument("cannot send to self");
		}
		if (seen_.find(peer_id) == seen_.end() && !find_connection(peer_id)) {
			throw std::runtime_error("unknown peer");
		}
		if (body.size() > kMaxFramePayload) {
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
	int udp_fd_ = -1;
	int listen_fd_ = -1;
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
		queue_frame(*connection, Text{pending.msg_id, pending.timestamp_ms, pending.body});
		return true;
	}

	void flush_queue(const std::string &peer_id) {
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
		const auto packet = encode_announce(announce);
		auto send_to = [&](const sockaddr_in &addr) {
			::sendto(
				udp_fd_,
				packet.data(),
				packet.size(),
				0,
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
		seen.info = info;
		if (seen.info.tcp_port == 0 && known_port != 0) {
			seen.info.tcp_port = known_port;
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
			const ssize_t n = ::recvfrom(
				udp_fd_,
				buffer,
				sizeof(buffer),
				0,
				reinterpret_cast<sockaddr *>(&from),
				&from_len);
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return;
				}
				throw_errno("recvfrom");
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
			remember_peer(info);
		}
	}

	void accept_tcp() {
		for (;;) {
			sockaddr_in from{};
			socklen_t from_len = sizeof(from);
			const int fd = ::accept(
				listen_fd_,
				reinterpret_cast<sockaddr *>(&from),
				&from_len);
			if (fd < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return;
				}
				throw_errno("accept");
			}
			set_nonblock(fd);
			Connection connection;
			connection.fd = fd;
			connection.ip = addr_ip(from);
			queue_frame(connection, Hello{
				config_.identity.peer_id,
				config_.identity.display_name});
			connection.hello_sent = true;
			connections_.push_back(std::move(connection));
		}
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
		const bool must_send = queued_.count(peer_id) && !queued_[peer_id].empty();
		if (!must_send && config_.identity.peer_id > peer_id) {
			return;
		}
		const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			return;
		}
		set_nonblock(fd);
		auto addr = make_addr(seen->second.info.ip, seen->second.info.tcp_port);
		const int rc = ::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
		if (rc < 0 && errno != EINPROGRESS && errno != EINTR) {
			::close(fd);
			return;
		}
		Connection connection;
		connection.fd = fd;
		connection.connecting = (rc < 0);
		connection.ip = seen->second.info.ip;
		connection.tcp_port = seen->second.info.tcp_port;
		connection.peer_id = peer_id;
		if (!connection.connecting) {
			queue_frame(connection, Hello{
				config_.identity.peer_id,
				config_.identity.display_name});
			connection.hello_sent = true;
		}
		connections_.push_back(std::move(connection));
	}

	void handle_connection(Connection &connection, short revents) {
		if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
			if (connection.connecting) {
				close_fd(connection.fd);
				return;
			}
		}
		if ((revents & POLLOUT) && connection.connecting) {
			int err = 0;
			socklen_t len = sizeof(err);
			if (::getsockopt(connection.fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
				close_fd(connection.fd);
				return;
			}
			connection.connecting = false;
			if (!connection.hello_sent) {
				queue_frame(connection, Hello{
					config_.identity.peer_id,
					config_.identity.display_name});
				connection.hello_sent = true;
			}
		}
		if ((revents & POLLOUT) && !connection.connecting && !connection.write_buf.empty()) {
			const ssize_t n = ::send(
				connection.fd,
				connection.write_buf.data(),
				connection.write_buf.size(),
				0);
			if (n < 0) {
				if (errno != EAGAIN && errno != EWOULDBLOCK) {
					close_fd(connection.fd);
				}
				return;
			}
			connection.write_buf.erase(
				connection.write_buf.begin(),
				connection.write_buf.begin() + n);
		}
		if (revents & POLLIN) {
			uint8_t buffer[4096];
			const ssize_t n = ::recv(connection.fd, buffer, sizeof(buffer), 0);
			if (n <= 0) {
				if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
					return;
				}
				close_fd(connection.fd);
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
				if (connection.fd < 0) {
					return;
				}
			}
		}
	}

	void on_message(Connection &connection, const Message &message) {
		if (const auto *hello = std::get_if<Hello>(&message)) {
			if (hello->peer_id == config_.identity.peer_id) {
				close_fd(connection.fd);
				return;
			}
			if (auto *existing = find_connection(hello->peer_id);
				existing && existing != &connection) {
				close_fd(connection.fd);
				return;
			}
			connection.peer_id = hello->peer_id;
			connection.display_name = hello->display_name;
			connection.hello_received = true;
			PeerInfo info;
			info.peer_id = hello->peer_id;
			info.display_name = hello->display_name;
			info.ip = connection.ip;
			info.tcp_port = connection.tcp_port;
			remember_peer(info);
			flush_queue(hello->peer_id);
			return;
		}
		if (!connection.hello_received) {
			close_fd(connection.fd);
			return;
		}
		if (const auto *text = std::get_if<Text>(&message)) {
			IncomingText incoming;
			incoming.from_peer_id = connection.peer_id;
			incoming.from_name = connection.display_name;
			incoming.msg_id = text->msg_id;
			incoming.timestamp_ms = text->timestamp_ms;
			incoming.body = text->body;
			texts_.push_back(std::move(incoming));
			queue_frame(connection, Ack{text->msg_id});
			return;
		}
		if (const auto *ack = std::get_if<Ack>(&message)) {
			acks_.push_back(ack->msg_id);
			return;
		}
		if (std::holds_alternative<Bye>(message)) {
			close_fd(connection.fd);
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

const Identity &Node::identity() const {
	return impl_->config_.identity;
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

} // namespace lan
