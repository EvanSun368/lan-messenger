#include "identity.h"
#include "node.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std::chrono_literals;

namespace {

void usage() {
	std::cerr << "Usage: lan_node --name <display-name> --dir <data-dir> "
		"[--bind <ipv4>] [--discovery-port <port>]\n"
		"         [--no-multicast] [--target <ipv4:udp-port>]\n"
		"Commands on stdin:\n"
		"  list\n"
		"  target <ipv4:udp-port>\n"
		"  send <peer-id-or-name> <text>\n"
		"  send <text>             (only when exactly one peer is visible)\n"
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
				<< " " << text.msg_id << " " << text.body << std::endl;
		}
		for (const auto &ack : node.take_acks()) {
			std::cout << "ack " << ack << std::endl;
		}

		pollfd in{STDIN_FILENO, POLLIN, 0};
		const int ready = ::poll(&in, 1, 0);
		if (ready > 0 && (in.revents & POLLIN)) {
			char buffer[1024];
			const ssize_t n = ::read(STDIN_FILENO, buffer, sizeof(buffer));
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
