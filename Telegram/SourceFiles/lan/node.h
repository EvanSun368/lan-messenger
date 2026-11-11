#pragma once

#include "identity.h"
#include "protocol.h"
#include "e2e.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lan {

struct PeerInfo {
	std::string peer_id;
	std::string display_name;
	std::string ip;
	uint16_t tcp_port = 0;
	std::vector<uint8_t> public_key;
};

struct IncomingText {
	std::string from_peer_id;
	std::string from_name;
	std::string msg_id;
	uint64_t timestamp_ms = 0;
	std::string body;
};

struct NodeConfig {
	Identity identity;
	// Empty disables multicast. Default is the LANLink discovery group.
	std::string multicast_group = "239.255.87.42";
	uint16_t discovery_port = 48710;
	// Interface used for bind and multicast. "0.0.0.0" listens on all.
	std::string bind_address = "0.0.0.0";
	uint16_t tcp_port = 0; // 0 = ephemeral
	uint16_t udp_port = 0; // 0 = use discovery_port, or ephemeral if 0 and no multicast?
	// Optional official tde2e key. If empty, the node generates an ephemeral key.
	std::shared_ptr<E2EKey> e2e_key;
	std::chrono::milliseconds announce_interval{1000};
	std::chrono::milliseconds peer_timeout{4000};
};

class Node {
public:
	explicit Node(NodeConfig config);
	~Node();

	Node(const Node &) = delete;
	Node &operator=(const Node &) = delete;

	void start();
	void stop();
	// Poll sockets and due timers, up to timeout.
	void step(std::chrono::milliseconds timeout);

	[[nodiscard]] const Identity &identity() const;
	[[nodiscard]] const std::vector<uint8_t> &public_key() const;
	[[nodiscard]] uint16_t tcp_port() const;
	[[nodiscard]] uint16_t udp_port() const;
	[[nodiscard]] std::vector<PeerInfo> peers() const;

	// Send announces also to this unicast address (used by tests,
	// and as a fallback when multicast is filtered).
	void add_discovery_target(const std::string &ip, uint16_t port);

	// Returns the message id. Throws if the peer is not known.
	std::string send_text(const std::string &peer_id, std::string_view body);

	std::vector<PeerInfo> take_peer_events();
	std::vector<IncomingText> take_texts();
	std::vector<std::string> take_acks();

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace lan
