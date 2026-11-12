#include "identity.h"
#include "node.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

using namespace std::chrono_literals;

namespace {

// True if stdin has bytes (or is closed). Used instead of a blocking read
// so the node can keep stepping. Windows cannot poll a console/pipe with
// WSAPoll, which only accepts sockets.
bool stdin_ready() {
#ifdef _WIN32
	const HANDLE handle = ::GetStdHandle(STD_INPUT_HANDLE);
	if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
		return false;
	}
	const DWORD type = ::GetFileType(handle);
	if (type == FILE_TYPE_PIPE) {
		DWORD available = 0;
		if (!::PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) {
			return true; // closed or broken: let the reader see EOF
		}
		return available > 0;
	}
	return ::WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
#else
	pollfd in{STDIN_FILENO, POLLIN, 0};
	return ::poll(&in, 1, 0) > 0 && (in.revents & POLLIN);
#endif
}

#ifdef _WIN32
int read_stdin(char *buffer, int size) {
	const HANDLE handle = ::GetStdHandle(STD_INPUT_HANDLE);
	DWORD n = 0;
	if (!::ReadFile(handle, buffer, static_cast<DWORD>(size), &n, nullptr)) {
		return -1;
	}
	return static_cast<int>(n);
}
#else
int read_stdin(char *buffer, int size) {
	const ssize_t n = ::read(STDIN_FILENO, buffer, static_cast<size_t>(size));
	return static_cast<int>(n);
}
#endif

void usage() {
	std::cerr << "Usage: lan_node --name <display-name> --dir <data-dir> "
		"[--bind <ipv4>] [--discovery-port <port>]\n"
		"         [--no-multicast] [--target <ipv4:udp-port>]\n"
		"Commands on stdin:\n"
		"  list\n"
		"  target <ipv4:udp-port>\n"
		"  send <peer-id-or-name> <text>\n"
		"  send <text>             (only when exactly one peer is visible)\n"
		"  send-file <peer-id-or-name> <path>\n"
		"  send-folder <peer-id-or-name> <dir>\n"
		"  group-create <name> [peer ...]\n"
		"  group-add <group-id> <peer>\n"
		"  group-remove <group-id> <peer>\n"
		"  group-send <group-id> <text>\n"
		"  groups\n"
		"  quit\n";
}

std::string find_peer_id(const lan::Node &node, const std::string &key) {
	if (lan::is_valid_peer_id(key)) {
		return key;
	}
	std::string match;
	int hits = 0;
	for (const auto &peer : node.peers()) {
		if (peer.display_name == key) {
			++hits;
			match = peer.peer_id;
		}
	}
	return hits == 1 ? match : std::string{};
}

bool handle_line(lan::Node &node, const std::string &line) {
	if (line == "quit" || line == "exit") {
		return false;
	}
	if (line.rfind("target ", 0) == 0) {
		const auto spec = line.substr(7);
		const auto colon = spec.rfind(':');
		if (colon == std::string::npos) {
			std::cout << "error target wants ipv4:port" << std::endl;
			return true;
		}
		try {
			const auto ip = spec.substr(0, colon);
			const auto port = static_cast<uint16_t>(std::stoi(spec.substr(colon + 1)));
			node.add_discovery_target(ip, port);
			std::cout << "target " << ip << ":" << port << std::endl;
		} catch (const std::exception &error) {
			std::cout << "error " << error.what() << std::endl;
		}
		return true;
	}
	if (line == "list") {
		const auto peers = node.peers();
		std::cout << "peers " << peers.size() << std::endl;
		for (const auto &peer : peers) {
			std::cout << "peer " << peer.peer_id << " " << peer.display_name
				<< " " << peer.ip << ":" << peer.tcp_port << std::endl;
		}
		return true;
	}
	if (line == "groups") {
		const auto groups = node.groups();
		std::cout << "groups " << groups.size() << std::endl;
		for (const auto &group : groups) {
			std::cout << "group " << group.group_id << " " << group.name
				<< " members " << group.member_ids.size() << std::endl;
		}
		return true;
	}
	if (line.rfind("group-create ", 0) == 0) {
		auto rest = line.substr(13);
		std::vector<std::string> members;
		std::string name;
		if (!rest.empty() && rest.front() == '"') {
			const auto end = rest.find('"', 1);
			if (end == std::string::npos) {
				std::cout << "error group-create wants a name" << std::endl;
				return true;
			}
			name = rest.substr(1, end - 1);
			rest = rest.substr(end + 1);
		} else {
			const auto space = rest.find(' ');
			name = space == std::string::npos ? rest : rest.substr(0, space);
			rest = space == std::string::npos ? std::string() : rest.substr(space + 1);
		}
		while (!rest.empty()) {
			while (!rest.empty() && rest.front() == ' ') {
				rest.erase(rest.begin());
			}
			if (rest.empty()) {
				break;
			}
			const auto space = rest.find(' ');
			const auto token = space == std::string::npos ? rest : rest.substr(0, space);
			rest = space == std::string::npos ? std::string() : rest.substr(space + 1);
			const auto peer_id = find_peer_id(node, token);
			if (peer_id.empty()) {
				std::cout << "error unknown peer" << std::endl;
				return true;
			}
			members.push_back(peer_id);
		}
		try {
			const auto id = node.create_group(name, members);
			std::cout << "group-created " << id << " " << name << std::endl;
		} catch (const std::exception &error) {
			std::cout << "error " << error.what() << std::endl;
		}
		return true;
	}
	if (line.rfind("group-add ", 0) == 0 || line.rfind("group-remove ", 0) == 0) {
		const bool adding = line.rfind("group-add ", 0) == 0;
		const auto rest = line.substr(adding ? 10 : 13);
		const auto space = rest.find(' ');
		if (space == std::string::npos) {
			std::cout << "error wants <group-id> <peer>" << std::endl;
			return true;
		}
		const auto group_id = rest.substr(0, space);
		const auto peer_id = find_peer_id(node, rest.substr(space + 1));
		if (peer_id.empty()) {
			std::cout << "error unknown peer" << std::endl;
			return true;
		}
		const bool ok = adding
			? node.add_group_member(group_id, peer_id)
			: node.remove_group_member(group_id, peer_id);
		std::cout << (ok ? (adding ? "group-added " : "group-removed ") : "error ")
			<< group_id << " " << peer_id << std::endl;
		return true;
	}
	if (line.rfind("group-send ", 0) == 0) {
		const auto rest = line.substr(11);
		const auto space = rest.find(' ');
		if (space == std::string::npos) {
			std::cout << "error group-send wants <group-id> <text>" << std::endl;
			return true;
		}
		try {
			const auto id = node.send_group_text(rest.substr(0, space), rest.substr(space + 1));
			std::cout << "sent-group " << rest.substr(0, space) << " " << id << std::endl;
		} catch (const std::exception &error) {
			std::cout << "error " << error.what() << std::endl;
		}
		return true;
	}
	if (line.rfind("send-folder ", 0) == 0) {
		const auto rest = line.substr(12);
		const auto space = rest.find(' ');
		if (space == std::string::npos) {
			std::cout << "error send-folder wants <peer> <dir>" << std::endl;
			return true;
		}
		const auto peer_id = find_peer_id(node, rest.substr(0, space));
		const auto path = rest.substr(space + 1);
		if (peer_id.empty()) {
			std::cout << "error unknown peer" << std::endl;
			return true;
		}
		try {
			const auto id = node.send_folder(peer_id, path);
			std::cout << "sent-folder " << peer_id << " " << id << " " << path << std::endl;
		} catch (const std::exception &error) {
			std::cout << "error " << error.what() << std::endl;
		}
		return true;
	}
	if (line.rfind("send-file ", 0) == 0) {
		const auto rest = line.substr(10);
		const auto space = rest.find(' ');
		if (space == std::string::npos) {
			std::cout << "error send-file wants <peer> <path>" << std::endl;
			return true;
		}
		const auto peer_id = find_peer_id(node, rest.substr(0, space));
		const auto path = rest.substr(space + 1);
		if (peer_id.empty()) {
			std::cout << "error unknown peer" << std::endl;
			return true;
		}
		try {
			const auto id = node.send_file(peer_id, path);
			std::cout << "sent-file " << peer_id << " " << id << " " << path << std::endl;
		} catch (const std::exception &error) {
			std::cout << "error " << error.what() << std::endl;
		}
		return true;
	}
	if (line.rfind("send ", 0) == 0) {
		const auto rest = line.substr(5);
		std::string peer_key;
		std::string body;
		const auto space = rest.find(' ');
		const auto peers = node.peers();
		if (space == std::string::npos) {
			if (peers.size() != 1) {
				std::cout << "error need peer id or a single visible peer" << std::endl;
				return true;
			}
			peer_key = peers[0].peer_id;
			body = rest;
		} else {
			peer_key = rest.substr(0, space);
			body = rest.substr(space + 1);
			if (!lan::is_valid_peer_id(peer_key) && find_peer_id(node, peer_key).empty()
				&& peers.size() == 1 && peer_key != peers[0].display_name) {
				// "send hello world" with one peer: treat the whole rest as body.
				peer_key = peers[0].peer_id;
				body = rest;
			}
		}
		const auto peer_id = find_peer_id(node, peer_key);
		if (peer_id.empty()) {
			std::cout << "error unknown peer" << std::endl;
			return true;
		}
		try {
			const auto msg_id = node.send_text(peer_id, body);
			std::cout << "sent " << peer_id << " " << msg_id << " " << body << std::endl;
		} catch (const std::exception &error) {
			std::cout << "error " << error.what() << std::endl;
		}
		return true;
	}
	std::cout << "error unknown command" << std::endl;
	return true;
}

} // namespace

int main(int argc, char **argv) {
	std::string name;
	std::filesystem::path dir;
	std::string bind = "0.0.0.0";
	uint16_t discovery_port = 48710;
	bool multicast = true;
	std::vector<std::pair<std::string, uint16_t>> targets;
	for (int i = 1; i < argc; ++i) {
		const auto arg = std::string(argv[i]);
		auto need = [&](const char *flag) -> std::string {
			if (i + 1 >= argc) {
				std::cerr << "missing value for " << flag << "\n";
				std::exit(2);
			}
			return argv[++i];
		};
		if (arg == "--name") {
			name = need("--name");
		} else if (arg == "--dir") {
			dir = need("--dir");
		} else if (arg == "--bind") {
			bind = need("--bind");
		} else if (arg == "--discovery-port") {
			discovery_port = static_cast<uint16_t>(std::stoi(need("--discovery-port")));
		} else if (arg == "--no-multicast") {
			multicast = false;
		} else if (arg == "--target") {
			const auto spec = need("--target");
			const auto colon = spec.rfind(':');
			if (colon == std::string::npos) {
				std::cerr << "--target wants ipv4:port\n";
				return 2;
			}
			targets.emplace_back(
				spec.substr(0, colon),
				static_cast<uint16_t>(std::stoi(spec.substr(colon + 1))));
		} else if (arg == "--help" || arg == "-h") {
			usage();
			return 0;
		} else {
			usage();
			return 2;
		}
	}
	if (name.empty() || dir.empty()) {
		usage();
		return 2;
	}

	lan::NodeConfig config;
	config.identity = lan::load_or_create_identity(dir, name);
	config.incoming_dir = dir / "incoming";
	config.state_dir = dir / "state";
	config.bind_address = bind;
	config.discovery_port = discovery_port;
	if (!multicast) {
		config.multicast_group.clear();
		config.udp_port = 0;
	}
	config.announce_interval = 300ms;
	config.peer_timeout = 3s;

	lan::Node node(std::move(config));
	node.start();
	for (const auto &target : targets) {
		node.add_discovery_target(target.first, target.second);
	}
	std::cout << "self " << node.identity().peer_id << " "
		<< node.identity().display_name
		<< " tcp=" << node.tcp_port()
		<< " udp=" << node.udp_port() << std::endl;
	std::cout.flush();

	std::string pending;
	bool running = true;
	while (running) {
		node.step(50ms);
		for (const auto &peer : node.take_peer_events()) {
			std::cout << "peer-up " << peer.peer_id << " " << peer.display_name
				<< " " << peer.ip << ":" << peer.tcp_port << std::endl;
		}
		for (const auto &text : node.take_texts()) {
			std::cout << "recv " << text.from_peer_id << " " << text.from_name
				<< " " << text.msg_id;
			if (!text.group_id.empty()) {
				std::cout << " group " << text.group_id;
			}
			std::cout << " " << text.body << std::endl;
		}
		for (const auto &ack : node.take_acks()) {
			std::cout << "ack " << ack << std::endl;
		}
		for (const auto &event : node.take_file_events()) {
			const char *kind = "file";
			if (event.kind == lan::FileEvent::Kind::Started) {
				kind = "file-start";
			} else if (event.kind == lan::FileEvent::Kind::Progress) {
				kind = "file-progress";
			} else if (event.kind == lan::FileEvent::Kind::Completed) {
				kind = "file-done";
			} else {
				kind = "file-fail";
			}
			std::cout << kind
				<< (event.outgoing ? " out " : " in ")
				<< event.peer_id << " " << event.transfer_id << " "
				<< event.filename << " " << event.bytes_done << "/" << event.size
				<< " " << event.path.string();
			if (!event.error.empty()) {
				std::cout << " " << event.error;
			}
			std::cout << std::endl;
		}
		for (const auto &event : node.take_group_events()) {
			std::cout << (event.kind == lan::GroupEvent::Kind::Removed
					? "group-removed "
					: "group-up ")
				<< event.group.group_id << " " << event.group.name
				<< " members " << event.group.member_ids.size() << std::endl;
		}

		if (stdin_ready()) {
			char buffer[1024];
			const int n = read_stdin(buffer, sizeof(buffer));
			if (n <= 0) {
				running = false;
				break;
			}
			pending.append(buffer, static_cast<size_t>(n));
			size_t pos = 0;
			while (true) {
				const auto nl = pending.find('\n', pos);
				if (nl == std::string::npos) {
					pending.erase(0, pos);
					break;
				}
				auto line = pending.substr(pos, nl - pos);
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				if (!line.empty()) {
					running = handle_line(node, line);
					if (!running) {
						break;
					}
				}
				pos = nl + 1;
			}
		}
	}
	node.stop();
	return 0;
}
