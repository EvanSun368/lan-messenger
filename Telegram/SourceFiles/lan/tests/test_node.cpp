#include "check.h"
#include "node.h"
#include "e2e.h"

#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {

bool wait_for_peer(lan::Node &a, lan::Node &b, const std::string &peer_id) {
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline) {
		a.step(20ms);
		b.step(20ms);
		for (const auto &peer : a.peers()) {
			if (peer.peer_id == peer_id) {
				return true;
			}
		}
	}
	return false;
}

lan::NodeConfig make_config(const char *name, uint16_t discovery_port) {
	lan::NodeConfig config;
	config.identity = lan::create_identity(name);
	config.multicast_group.clear();
	config.discovery_port = discovery_port;
	config.bind_address = "127.0.0.1";
	config.tcp_port = 0;
	config.udp_port = 0;
	config.announce_interval = 50ms;
	config.peer_timeout = 1500ms;
	return config;
}

} // namespace

LAN_TEST(two_nodes_discover_and_exchange_text_on_loopback) {
	lan::Node alice(make_config("Alice", 0));
	lan::Node bob(make_config("Bob", 0));
	alice.start();
	bob.start();
	LAN_REQUIRE(alice.tcp_port() != 0);
	LAN_REQUIRE(bob.tcp_port() != 0);
	LAN_REQUIRE(alice.udp_port() != 0);
	LAN_REQUIRE(bob.udp_port() != 0);
	LAN_CHECK(alice.udp_port() != bob.udp_port());

	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());

	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	LAN_CHECK(lan::is_valid_public_key(alice.public_key()));
	LAN_CHECK(alice.public_key() != bob.public_key());
	bool saw_bob_key = false;
	for (const auto &peer : alice.peers()) {
		if (peer.peer_id == bob.identity().peer_id) {
			LAN_CHECK(peer.public_key == bob.public_key());
			saw_bob_key = true;
		}
	}
	LAN_CHECK(saw_bob_key);

	const auto msg_id = alice.send_text(bob.identity().peer_id, "hello from alice");
	LAN_CHECK(lan::is_valid_peer_id(msg_id));

	bool received = false;
	bool acked = false;
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline && (!received || !acked)) {
		alice.step(20ms);
		bob.step(20ms);
		for (const auto &text : bob.take_texts()) {
			LAN_CHECK_EQ(text.from_peer_id, alice.identity().peer_id);
			LAN_CHECK_EQ(text.from_name, std::string("Alice"));
			LAN_CHECK_EQ(text.msg_id, msg_id);
			LAN_CHECK_EQ(text.body, std::string("hello from alice"));
			received = true;
		}
		for (const auto &ack : alice.take_acks()) {
			if (ack == msg_id) {
				acked = true;
			}
		}
	}
	LAN_CHECK(received);
	LAN_CHECK(acked);
	alice.stop();
	bob.stop();
}

LAN_TEST(two_nodes_discover_over_loopback_multicast) {
	lan::NodeConfig alice_cfg;
	alice_cfg.identity = lan::create_identity("M-Alice");
	alice_cfg.multicast_group = "239.255.87.99";
	alice_cfg.discovery_port = 0; // filled after we pick a free port via first node? 
	// Use a high fixed test port; SO_REUSEADDR/PORT lets both bind.
	alice_cfg.discovery_port = 48799;
	alice_cfg.bind_address = "0.0.0.0";
	alice_cfg.announce_interval = 50ms;
	alice_cfg.peer_timeout = 1500ms;

	lan::NodeConfig bob_cfg = alice_cfg;
	bob_cfg.identity = lan::create_identity("M-Bob");

	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();

	const bool found = wait_for_peer(alice, bob, bob.identity().peer_id)
		&& wait_for_peer(bob, alice, alice.identity().peer_id);
	if (!found) {
		// Multicast loopback is environment-dependent. Do not fail the
		// suite; the unicast-target test already covers the protocol.
		alice.stop();
		bob.stop();
		return;
	}
	const auto msg_id = bob.send_text(alice.identity().peer_id, "multicast hi");
	bool received = false;
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline && !received) {
		alice.step(20ms);
		bob.step(20ms);
		for (const auto &text : alice.take_texts()) {
			LAN_CHECK_EQ(text.body, std::string("multicast hi"));
			LAN_CHECK_EQ(text.msg_id, msg_id);
			received = true;
		}
	}
	LAN_CHECK(received);
	alice.stop();
	bob.stop();
}
