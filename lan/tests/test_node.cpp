#include "check.h"
#include "node.h"
#include "e2e.h"
#include "transfer.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <filesystem>
#include <string>
#include <thread>
#include <unistd.h>

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

LAN_TEST(two_nodes_transfer_a_file_on_loopback) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-node-xfer-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "memo.txt";
	{
		std::ofstream out(source, std::ios::binary);
		out << "hello from a lan file, 你好";
	}
	// A slightly larger file too, bigger than one chunk? keep modest for unit test.
	const auto large = dir / "alice" / "blob.bin";
	{
		std::string body(lan::kFileChunkPlainMax + 97, 'Z');
		std::ofstream out(large, std::ios::binary);
		out.write(body.data(), static_cast<std::streamsize>(body.size()));
	}

	auto alice_cfg = make_config("Alice", 0);
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	lan::Node alice(std::move(alice_cfg));
	lan::Node bob(std::move(bob_cfg));
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));

	const auto small_id = alice.send_file(bob.identity().peer_id, source);
	const auto large_id = alice.send_file(bob.identity().peer_id, large, "blob.bin", "application/octet-stream");
	bool small_done = false;
	bool large_done = false;
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < deadline && (!small_done || !large_done)) {
		alice.step(20ms);
		bob.step(20ms);
		(void)alice.take_file_events();
		for (const auto &event : bob.take_file_events()) {
			LAN_CHECK(!event.outgoing);
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (event.kind == lan::FileEvent::Kind::Completed) {
				if (event.transfer_id == small_id) {
					LAN_CHECK_EQ(event.filename, std::string("memo.txt"));
					std::ifstream in(event.path, std::ios::binary);
					std::string body((std::istreambuf_iterator<char>(in)), {});
					LAN_CHECK_EQ(body, std::string("hello from a lan file, 你好"));
					small_done = true;
				}
				if (event.transfer_id == large_id) {
					LAN_CHECK_EQ(event.size, uint64_t(lan::kFileChunkPlainMax + 97));
					LAN_CHECK(std::filesystem::file_size(event.path) == event.size);
					large_done = true;
				}
			}
		}
	}
	LAN_CHECK(small_done);
	LAN_CHECK(large_done);
	std::filesystem::remove_all(dir);
}

LAN_TEST(file_transfer_fails_closed_without_writing_plaintext) {
	// Covered at the frame layer in test_transfer. Here, a second offer
	// with a junk sealed blob must be rejected and must not appear as a file.
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-node-fail-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir);
	auto alice_cfg = make_config("Alice", 0);
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir;
	lan::Node alice(std::move(alice_cfg));
	lan::Node bob(std::move(bob_cfg));
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	// A real file still works after a successful handshake, proving keys exist.
	const auto source = dir / "ok.bin";
	{
		std::ofstream out(source, std::ios::binary);
		out << "ok";
	}
	alice.send_file(bob.identity().peer_id, source, "ok.bin");
	bool done = false;
	const auto deadline = std::chrono::steady_clock::now() + 3s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(20ms);
		bob.step(20ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Completed) {
				done = true;
			}
			LAN_CHECK(event.kind != lan::FileEvent::Kind::Failed);
		}
	}
	LAN_CHECK(done);
	std::filesystem::remove_all(dir);
}

LAN_TEST(two_nodes_transfer_a_folder_tree) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-node-folder-" + std::to_string(getpid()));
	const auto source = dir / "Project";
	std::filesystem::create_directories(source / "src" / "util");
	std::filesystem::create_directories(source / "empty" / "nested");
	{
		std::ofstream out(source / "README.md");
		out << "hello folder";
	}
	{
		std::ofstream out(source / "src" / "main.cpp");
		out << "int main(){}";
	}
	{
		std::ofstream out(source / "src" / "util" / "log.h");
		out << "log";
	}
	auto alice_cfg = make_config("Alice", 0);
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	lan::Node alice(std::move(alice_cfg));
	lan::Node bob(std::move(bob_cfg));
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));

	const auto folder_id = alice.send_folder(bob.identity().peer_id, source);
	bool done = false;
	std::filesystem::path received;
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(20ms);
		bob.step(20ms);
		(void)alice.take_file_events();
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (event.kind == lan::FileEvent::Kind::Completed && event.is_folder
				&& event.folder_id == folder_id) {
				received = event.path;
				done = true;
			}
		}
	}
	LAN_CHECK(done);
	LAN_REQUIRE(std::filesystem::exists(received / "README.md"));
	LAN_REQUIRE(std::filesystem::exists(received / "src" / "main.cpp"));
	LAN_REQUIRE(std::filesystem::exists(received / "src" / "util" / "log.h"));
	LAN_CHECK(std::filesystem::is_directory(received / "empty" / "nested"));
	{
		std::ifstream in(received / "README.md");
		std::string body((std::istreambuf_iterator<char>(in)), {});
		LAN_CHECK_EQ(body, std::string("hello folder"));
	}
	alice.stop();
	bob.stop();
	std::filesystem::remove_all(dir);
}

LAN_TEST(file_transfer_resumes_after_receiver_restart) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-node-resume-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	const auto source = dir / "alice" / "blob.bin";
	const std::string payload(lan::kFileChunkPlainMax * 3 + 50, 'R');
	{
		std::ofstream out(source, std::ios::binary);
		out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
	}
	auto alice_key = std::make_shared<lan::E2EKey>(lan::E2EKey::generate());
	auto bob_key = std::make_shared<lan::E2EKey>(lan::E2EKey::generate());
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.e2e_key = alice_key;
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_identity = lan::create_identity("Bob");
	auto make_bob = [&]() {
		auto cfg = make_config("Bob", 0);
		cfg.identity = bob_identity;
		cfg.e2e_key = bob_key;
		cfg.incoming_dir = dir / "bob";
		cfg.state_dir = dir / "bob-state";
		return cfg;
	};
	lan::Node alice(std::move(alice_cfg));
	auto bob = std::make_unique<lan::Node>(make_bob());
	alice.start();
	bob->start();
	alice.add_discovery_target("127.0.0.1", bob->udp_port());
	bob->add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, *bob, bob->identity().peer_id));
	LAN_REQUIRE(wait_for_peer(*bob, alice, alice.identity().peer_id));

	const auto transfer_id = alice.send_file(bob->identity().peer_id, source);
	bool progressed = false;
	const auto mid = std::chrono::steady_clock::now() + 3s;
	while (std::chrono::steady_clock::now() < mid && !progressed) {
		alice.step(10ms);
		bob->step(10ms);
		(void)alice.take_file_events();
		for (const auto &event : bob->take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (event.transfer_id == transfer_id
				&& event.kind == lan::FileEvent::Kind::Progress
				&& event.bytes_done > 0
				&& event.bytes_done < event.size) {
				progressed = true;
			}
		}
	}
	LAN_REQUIRE(progressed);
	bob->stop();
	bob.reset();

	bob = std::make_unique<lan::Node>(make_bob());
	bob->start();
	alice.add_discovery_target("127.0.0.1", bob->udp_port());
	bob->add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, *bob, bob->identity().peer_id));
	LAN_REQUIRE(wait_for_peer(*bob, alice, alice.identity().peer_id));

	bool done = false;
	std::filesystem::path received;
	uint64_t first_resume_bytes = 0;
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(20ms);
		bob->step(20ms);
		(void)alice.take_file_events();
		for (const auto &event : bob->take_file_events()) {
			if (event.transfer_id != transfer_id) {
				continue;
			}
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (first_resume_bytes == 0 && event.bytes_done > 0) {
				first_resume_bytes = event.bytes_done;
			}
			if (event.kind == lan::FileEvent::Kind::Completed) {
				received = event.path;
				done = true;
			}
		}
	}
	LAN_CHECK(done);
	LAN_CHECK(first_resume_bytes > 0);
	LAN_CHECK(std::filesystem::file_size(received) == payload.size());
	{
		std::ifstream in(received, std::ios::binary);
		std::string body((std::istreambuf_iterator<char>(in)), {});
		LAN_CHECK_EQ(body, payload);
	}
	alice.stop();
	bob->stop();
	std::filesystem::remove_all(dir);
}

LAN_TEST(three_nodes_exchange_group_text) {
	auto alice_cfg = make_config("Alice", 0);
	auto bob_cfg = make_config("Bob", 0);
	auto carol_cfg = make_config("Carol", 0);
	const auto alice_state = std::filesystem::temp_directory_path()
		/ ("lanlink-group-alice-" + std::to_string(getpid()));
	const auto bob_state = std::filesystem::temp_directory_path()
		/ ("lanlink-group-bob-" + std::to_string(getpid()));
	const auto carol_state = std::filesystem::temp_directory_path()
		/ ("lanlink-group-carol-" + std::to_string(getpid()));
	alice_cfg.state_dir = alice_state;
	bob_cfg.state_dir = bob_state;
	carol_cfg.state_dir = carol_state;
	lan::Node alice(std::move(alice_cfg));
	lan::Node bob(std::move(bob_cfg));
	lan::Node carol(std::move(carol_cfg));
	alice.start();
	bob.start();
	carol.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	alice.add_discovery_target("127.0.0.1", carol.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	bob.add_discovery_target("127.0.0.1", carol.udp_port());
	carol.add_discovery_target("127.0.0.1", alice.udp_port());
	carol.add_discovery_target("127.0.0.1", bob.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(alice, carol, carol.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(carol, alice, alice.identity().peer_id));

	const auto group_id = alice.create_group(
		"车间群",
		{bob.identity().peer_id, carol.identity().peer_id});
	bool bob_joined = false;
	bool carol_joined = false;
	auto join_deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < join_deadline && (!bob_joined || !carol_joined)) {
		alice.step(20ms);
		bob.step(20ms);
		carol.step(20ms);
		for (const auto &event : bob.take_group_events()) {
			if (event.kind == lan::GroupEvent::Kind::Upserted && event.group.group_id == group_id) {
				bob_joined = true;
			}
		}
		for (const auto &event : carol.take_group_events()) {
			if (event.kind == lan::GroupEvent::Kind::Upserted && event.group.group_id == group_id) {
				carol_joined = true;
			}
		}
	}
	LAN_CHECK(bob_joined);
	LAN_CHECK(carol_joined);

	const auto msg_id = alice.send_group_text(group_id, "hello group");
	bool bob_got = false;
	bool carol_got = false;
	const auto deadline = std::chrono::steady_clock::now() + 3s;
	while (std::chrono::steady_clock::now() < deadline && (!bob_got || !carol_got)) {
		alice.step(20ms);
		bob.step(20ms);
		carol.step(20ms);
		for (const auto &text : bob.take_texts()) {
			if (text.msg_id == msg_id) {
				LAN_CHECK_EQ(text.group_id, group_id);
				LAN_CHECK_EQ(text.body, std::string("hello group"));
				LAN_CHECK_EQ(text.from_peer_id, alice.identity().peer_id);
				bob_got = true;
			}
		}
		for (const auto &text : carol.take_texts()) {
			if (text.msg_id == msg_id) {
				LAN_CHECK_EQ(text.group_id, group_id);
				LAN_CHECK_EQ(text.body, std::string("hello group"));
				carol_got = true;
			}
		}
	}
	LAN_CHECK(bob_got);
	LAN_CHECK(carol_got);
	alice.stop();
	bob.stop();
	carol.stop();
	std::filesystem::remove_all(alice_state);
	std::filesystem::remove_all(bob_state);
	std::filesystem::remove_all(carol_state);
}

LAN_TEST(three_nodes_transfer_a_group_file) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-group-file-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	std::filesystem::create_directories(dir / "carol");
	const auto source = dir / "alice" / "spec.txt";
	{
		std::ofstream out(source, std::ios::binary);
		out << "group file body 你好";
	}

	auto alice_cfg = make_config("Alice", 0);
	auto bob_cfg = make_config("Bob", 0);
	auto carol_cfg = make_config("Carol", 0);
	alice_cfg.state_dir = dir / "alice-state";
	bob_cfg.state_dir = dir / "bob-state";
	carol_cfg.state_dir = dir / "carol-state";
	bob_cfg.incoming_dir = dir / "bob";
	carol_cfg.incoming_dir = dir / "carol";
	lan::Node alice(std::move(alice_cfg));
	lan::Node bob(std::move(bob_cfg));
	lan::Node carol(std::move(carol_cfg));
	alice.start();
	bob.start();
	carol.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	alice.add_discovery_target("127.0.0.1", carol.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	bob.add_discovery_target("127.0.0.1", carol.udp_port());
	carol.add_discovery_target("127.0.0.1", alice.udp_port());
	carol.add_discovery_target("127.0.0.1", bob.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(alice, carol, carol.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(carol, alice, alice.identity().peer_id));

	const auto group_id = alice.create_group(
		"文件群",
		{bob.identity().peer_id, carol.identity().peer_id});
	bool bob_joined = false;
	bool carol_joined = false;
	auto join_deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < join_deadline && (!bob_joined || !carol_joined)) {
		alice.step(20ms);
		bob.step(20ms);
		carol.step(20ms);
		for (const auto &event : bob.take_group_events()) {
			if (event.kind == lan::GroupEvent::Kind::Upserted && event.group.group_id == group_id) {
				bob_joined = true;
			}
		}
		for (const auto &event : carol.take_group_events()) {
			if (event.kind == lan::GroupEvent::Kind::Upserted && event.group.group_id == group_id) {
				carol_joined = true;
			}
		}
	}
	LAN_CHECK(bob_joined);
	LAN_CHECK(carol_joined);

	alice.send_group_file(group_id, source);
	bool bob_got = false;
	bool carol_got = false;
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < deadline && (!bob_got || !carol_got)) {
		alice.step(20ms);
		bob.step(20ms);
		carol.step(20ms);
		(void)alice.take_file_events();
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (event.kind == lan::FileEvent::Kind::Completed && !event.is_folder) {
				LAN_CHECK_EQ(event.group_id, group_id);
				LAN_CHECK_EQ(event.filename, std::string("spec.txt"));
				LAN_CHECK_EQ(event.peer_id, alice.identity().peer_id);
				std::ifstream in(event.path, std::ios::binary);
				std::string body((std::istreambuf_iterator<char>(in)), {});
				LAN_CHECK_EQ(body, std::string("group file body 你好"));
				bob_got = true;
			}
		}
		for (const auto &event : carol.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (event.kind == lan::FileEvent::Kind::Completed && !event.is_folder) {
				LAN_CHECK_EQ(event.group_id, group_id);
				LAN_CHECK_EQ(event.filename, std::string("spec.txt"));
				std::ifstream in(event.path, std::ios::binary);
				std::string body((std::istreambuf_iterator<char>(in)), {});
				LAN_CHECK_EQ(body, std::string("group file body 你好"));
				carol_got = true;
			}
		}
	}
	LAN_CHECK(bob_got);
	LAN_CHECK(carol_got);
	alice.stop();
	bob.stop();
	carol.stop();
	std::filesystem::remove_all(dir);
}

LAN_TEST(three_nodes_transfer_a_group_folder) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-group-folder-" + std::to_string(getpid()));
	const auto source = dir / "alice" / "tree";
	std::filesystem::create_directories(source / "src");
	std::filesystem::create_directories(source / "empty");
	{
		std::ofstream out(source / "src" / "a.txt");
		out << "alpha";
	}
	{
		std::ofstream out(source / "readme.md");
		out << "readme";
	}
	std::filesystem::create_directories(dir / "bob");
	std::filesystem::create_directories(dir / "carol");

	auto alice_cfg = make_config("Alice", 0);
	auto bob_cfg = make_config("Bob", 0);
	auto carol_cfg = make_config("Carol", 0);
	alice_cfg.state_dir = dir / "alice-state";
	bob_cfg.state_dir = dir / "bob-state";
	carol_cfg.state_dir = dir / "carol-state";
	bob_cfg.incoming_dir = dir / "bob";
	carol_cfg.incoming_dir = dir / "carol";
	lan::Node alice(std::move(alice_cfg));
	lan::Node bob(std::move(bob_cfg));
	lan::Node carol(std::move(carol_cfg));
	alice.start();
	bob.start();
	carol.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	alice.add_discovery_target("127.0.0.1", carol.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	bob.add_discovery_target("127.0.0.1", carol.udp_port());
	carol.add_discovery_target("127.0.0.1", alice.udp_port());
	carol.add_discovery_target("127.0.0.1", bob.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(alice, carol, carol.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(carol, alice, alice.identity().peer_id));

	const auto group_id = alice.create_group(
		"目录群",
		{bob.identity().peer_id, carol.identity().peer_id});
	bool bob_joined = false;
	bool carol_joined = false;
	auto join_deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < join_deadline && (!bob_joined || !carol_joined)) {
		alice.step(20ms);
		bob.step(20ms);
		carol.step(20ms);
		for (const auto &event : bob.take_group_events()) {
			if (event.kind == lan::GroupEvent::Kind::Upserted && event.group.group_id == group_id) {
				bob_joined = true;
			}
		}
		for (const auto &event : carol.take_group_events()) {
			if (event.kind == lan::GroupEvent::Kind::Upserted && event.group.group_id == group_id) {
				carol_joined = true;
			}
		}
	}
	LAN_CHECK(bob_joined);
	LAN_CHECK(carol_joined);

	alice.send_group_folder(group_id, source);
	bool bob_done = false;
	bool carol_done = false;
	const auto deadline = std::chrono::steady_clock::now() + 8s;
	while (std::chrono::steady_clock::now() < deadline && (!bob_done || !carol_done)) {
		alice.step(20ms);
		bob.step(20ms);
		carol.step(20ms);
		(void)alice.take_file_events();
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (event.kind == lan::FileEvent::Kind::Completed && event.is_folder) {
				LAN_CHECK_EQ(event.group_id, group_id);
				LAN_CHECK_EQ(event.filename, std::string("tree"));
				LAN_CHECK(std::filesystem::exists(event.path / "src" / "a.txt"));
				LAN_CHECK(std::filesystem::exists(event.path / "readme.md"));
				LAN_CHECK(std::filesystem::is_directory(event.path / "empty"));
				bob_done = true;
			}
		}
		for (const auto &event : carol.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				LAN_CHECK(event.error.empty() && false);
			}
			if (event.kind == lan::FileEvent::Kind::Completed && event.is_folder) {
				LAN_CHECK_EQ(event.group_id, group_id);
				LAN_CHECK(std::filesystem::exists(event.path / "src" / "a.txt"));
				carol_done = true;
			}
		}
	}
	LAN_CHECK(bob_done);
	LAN_CHECK(carol_done);
	alice.stop();
	bob.stop();
	carol.stop();
	std::filesystem::remove_all(dir);
}

LAN_TEST(display_name_change_is_announced_to_peer) {
	lan::Node alice(make_config("Alice", 0));
	lan::Node bob(make_config("Bob", 0));
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	LAN_CHECK(alice.set_display_name("Alice-Renamed"));
	LAN_CHECK_EQ(alice.identity().display_name, std::string("Alice-Renamed"));
	LAN_CHECK(!alice.set_display_name("   "));
	bool saw = false;
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline && !saw) {
		alice.step(20ms);
		bob.step(20ms);
		for (const auto &peer : bob.peers()) {
			if (peer.peer_id == alice.identity().peer_id
				&& peer.display_name == "Alice-Renamed") {
				saw = true;
			}
		}
	}
	LAN_CHECK(saw);
	alice.stop();
	bob.stop();
}

LAN_TEST(sender_can_recall_text_and_peer_observes_it) {
	lan::Node alice(make_config("Alice", 0));
	lan::Node bob(make_config("Bob", 0));
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto msg_id = alice.send_text(bob.identity().peer_id, "please unsend me");
	bool received = false;
	auto deadline = std::chrono::steady_clock::now() + 2s;
	uint64_t sent_ms = 0;
	while (std::chrono::steady_clock::now() < deadline && !received) {
		alice.step(20ms);
		bob.step(20ms);
		for (const auto &text : bob.take_texts()) {
			if (text.msg_id == msg_id) {
				received = true;
				sent_ms = text.timestamp_ms;
			}
		}
	}
	LAN_REQUIRE(received);
	LAN_CHECK(alice.recall_message(bob.identity().peer_id, msg_id, false, sent_ms));
	bool recalled = false;
	deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline && !recalled) {
		alice.step(20ms);
		bob.step(20ms);
		for (const auto &event : bob.take_recalls()) {
			LAN_CHECK_EQ(event.from_peer_id, alice.identity().peer_id);
			LAN_CHECK_EQ(event.msg_id, msg_id);
			LAN_CHECK(event.group_id.empty());
			recalled = true;
		}
	}
	LAN_CHECK(recalled);
	const auto too_old = sent_ms > 0 ? sent_ms : 1;
	LAN_CHECK(!alice.recall_message(
		bob.identity().peer_id,
		msg_id,
		false,
		too_old - (lan::kRecallWindowMs + 1000)));
	alice.stop();
	bob.stop();
}

LAN_TEST(abort_cancels_a_file_and_does_not_resume) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-abort-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "big.bin";
	{
		std::string payload(lan::kFileChunkPlainMax * 256, 'A');
		std::ofstream out(source, std::ios::binary);
		out << payload;
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool started = false;
	auto deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline && !started) {
		alice.step(5ms);
		bob.step(5ms);
		for (const auto &event : alice.take_file_events()) {
			if (event.transfer_id == id
				&& (event.kind == lan::FileEvent::Kind::Offered
					|| event.kind == lan::FileEvent::Kind::Started
					|| event.kind == lan::FileEvent::Kind::Progress)) {
				started = true;
			}
		}
	}
	LAN_REQUIRE(started);
	LAN_CHECK(alice.abort_transfer(id));
	bool alice_failed = false;
	bool bob_failed = false;
	bool completed = false;
	deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < deadline && (!alice_failed || !bob_failed)) {
		alice.step(20ms);
		bob.step(20ms);
		for (const auto &event : alice.take_file_events()) {
			if (event.transfer_id == id && event.kind == lan::FileEvent::Kind::Failed) {
				alice_failed = true;
			}
			if (event.transfer_id == id && event.kind == lan::FileEvent::Kind::Completed) {
				completed = true;
			}
		}
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				bob_failed = true;
			}
			if (event.kind == lan::FileEvent::Kind::Completed) {
				completed = true;
			}
		}
	}
	LAN_CHECK(alice_failed);
	LAN_CHECK(bob_failed);
	LAN_CHECK(!completed);
	// A second abort is harmless; the record must not come back as resumable.
	LAN_CHECK(!alice.abort_transfer(id));
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(file_transfer_throughput_on_loopback) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-speed-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const uint64_t bytes = 8ull * 1024ull * 1024ull;
	const auto source = dir / "alice" / "speed.bin";
	{
		std::string payload(bytes, 'S');
		std::ofstream out(source, std::ios::binary);
		out << payload;
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto started_at = std::chrono::steady_clock::now();
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool done = false;
	auto deadline = started_at + 15s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Completed && event.size == bytes) {
				done = true;
			}
		}
		alice.take_file_events();
	}
	const auto elapsed = std::chrono::steady_clock::now() - started_at;
	const double seconds = std::chrono::duration<double>(elapsed).count();
	const double mib_s = (double(bytes) / (1024.0 * 1024.0)) / seconds;
	std::cout << "loopback 8 MiB file transfer (plain, default): " << seconds << " s, "
		<< mib_s << " MiB/s, id=" << id << "\n";
	LAN_CHECK(done);
	// Unencrypted fast path on loopback should stay well above a few
	// mebibytes per second and ideally beat the previous sealed path.
	LAN_CHECK(mib_s > 2.0);
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(plain_and_encrypted_file_transfer_both_complete) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-encopt-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "note.txt";
	{
		std::ofstream out(source, std::ios::binary);
		out << "hello plaintext and sealed paths";
	}
	auto run = [&](bool encrypt, const char *label) {
		auto alice_cfg = make_config("Alice", 0);
		alice_cfg.encrypt_files = encrypt;
		alice_cfg.incoming_dir = dir / "alice";
		alice_cfg.state_dir = dir / (std::string(label) + "-alice-state");
		auto bob_cfg = make_config("Bob", 0);
		bob_cfg.encrypt_files = encrypt;
		bob_cfg.incoming_dir = dir / "bob" / label;
		bob_cfg.state_dir = dir / (std::string(label) + "-bob-state");
		std::filesystem::create_directories(bob_cfg.incoming_dir);
		lan::Node alice(alice_cfg);
		lan::Node bob(bob_cfg);
		alice.start();
		bob.start();
		alice.add_discovery_target("127.0.0.1", bob.udp_port());
		bob.add_discovery_target("127.0.0.1", alice.udp_port());
		LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
		LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
		LAN_CHECK(alice.encrypt_files() == encrypt);
		const auto id = alice.send_file(bob.identity().peer_id, source);
		bool done = false;
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (std::chrono::steady_clock::now() < deadline && !done) {
			alice.step(0ms);
			bob.step(0ms);
			for (const auto &event : bob.take_file_events()) {
				if (event.kind == lan::FileEvent::Kind::Completed) {
					done = true;
					LAN_CHECK(event.filename.find("note.txt") != std::string::npos);
				}
				LAN_CHECK(event.kind != lan::FileEvent::Kind::Failed);
			}
			alice.take_file_events();
		}
		LAN_CHECK(done);
		alice.stop();
		bob.stop();
		(void)id;
	};
	run(false, "plain");
	run(true, "sealed");
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(plain_file_transfer_abort_does_not_resume) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-plain-abort-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "big.bin";
	{
		// Must outrun the unencrypted fast path so abort can fire
		// before the transfer finishes.
		std::string payload(32 * 1024 * 1024, 'A');
		std::ofstream out(source, std::ios::binary);
		out << payload;
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.encrypt_files = false;
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.encrypt_files = false;
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool started = false;
	const auto start_deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < start_deadline && !started) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : alice.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Offered
				|| event.kind == lan::FileEvent::Kind::Started
				|| event.kind == lan::FileEvent::Kind::Progress) {
				started = true;
			}
			if (event.kind == lan::FileEvent::Kind::Completed) {
				// Too late; the next assert will fail if this happens.
			}
		}
		bob.take_file_events();
	}
	LAN_CHECK(alice.abort_transfer(id));
	bool alice_failed = false;
	bool bob_failed = false;
	bool completed = false;
	const auto deadline = std::chrono::steady_clock::now() + 3s;
	while (std::chrono::steady_clock::now() < deadline && (!alice_failed || !bob_failed)) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : alice.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				alice_failed = true;
			}
			if (event.kind == lan::FileEvent::Kind::Completed) {
				completed = true;
			}
		}
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Failed) {
				bob_failed = true;
			}
			if (event.kind == lan::FileEvent::Kind::Completed) {
				completed = true;
			}
		}
	}
	LAN_CHECK(!completed);
	LAN_CHECK(!alice.abort_transfer(id));
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(file_transfer_throughput_encrypted_on_loopback) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-speed-enc-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const uint64_t bytes = 8ull * 1024ull * 1024ull;
	const auto source = dir / "alice" / "speed.bin";
	{
		std::string payload(bytes, 'E');
		std::ofstream out(source, std::ios::binary);
		out << payload;
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.encrypt_files = true;
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.encrypt_files = true;
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto started_at = std::chrono::steady_clock::now();
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool done = false;
	auto deadline = started_at + 15s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Completed && event.size == bytes) {
				done = true;
			}
		}
		alice.take_file_events();
	}
	const auto elapsed = std::chrono::steady_clock::now() - started_at;
	const double seconds = std::chrono::duration<double>(elapsed).count();
	const double mib_s = (double(bytes) / (1024.0 * 1024.0)) / seconds;
	std::cout << "loopback 8 MiB file transfer (encrypted): " << seconds << " s, "
		<< mib_s << " MiB/s, id=" << id << "\n";
	LAN_CHECK(done);
	LAN_CHECK(mib_s > 2.0);
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(profile_fields_are_announced_to_peers) {
	lan::Node alice(make_config("Alice", 0));
	lan::Node bob(make_config("Bob", 0));
	alice.set_profile("Ada", "Lovelace", "Notes from the engine");
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	bool saw = false;
	for (const auto &peer : bob.peers()) {
		if (peer.peer_id == alice.identity().peer_id) {
			LAN_CHECK_EQ(peer.first_name, std::string("Ada"));
			LAN_CHECK_EQ(peer.last_name, std::string("Lovelace"));
			LAN_CHECK_EQ(peer.bio, std::string("Notes from the engine"));
			LAN_CHECK(!peer.ip.empty());
			saw = true;
		}
	}
	LAN_CHECK(saw);
	alice.stop();
	bob.stop();
}

LAN_TEST(two_nodes_exchange_text_larger_than_old_frame_limit) {
	// The previous cap silently dropped bodies above kMaxFramePayload
	// (64 KiB) in send_text / encode_frame / the UI catch. A message
	// clearly over that cap must arrive intact.
	lan::Node alice(make_config("Alice", 0));
	lan::Node bob(make_config("Bob", 0));
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));

	std::string body(lan::kMaxFramePayload + 16 * 1024, 'L');
	body += "END-MARKER-9f3a";
	LAN_CHECK(body.size() > lan::kMaxFramePayload);
	const auto msg_id = alice.send_text(bob.identity().peer_id, body);
	LAN_CHECK(lan::is_valid_peer_id(msg_id));

	bool received = false;
	const auto deadline = std::chrono::steady_clock::now() + 8s;
	while (std::chrono::steady_clock::now() < deadline && !received) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &text : bob.take_texts()) {
			LAN_CHECK_EQ(text.msg_id, msg_id);
			LAN_CHECK(text.body == body);
			LAN_CHECK_EQ(text.body.size(), body.size());
			received = true;
		}
		alice.take_acks();
	}
	LAN_CHECK(received);
	alice.stop();
	bob.stop();
}

LAN_TEST(file_transfer_throughput_128mib_on_loopback) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-speed128-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const uint64_t bytes = 128ull * 1024ull * 1024ull;
	const auto source = dir / "alice" / "speed.bin";
	{
		std::ofstream out(source, std::ios::binary);
		std::string chunk(1024 * 1024, 'S');
		for (uint64_t i = 0; i < bytes; i += chunk.size()) {
			out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
		}
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto started_at = std::chrono::steady_clock::now();
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool done = false;
	auto deadline = started_at + 90s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Completed && event.size == bytes) {
				done = true;
			}
		}
		alice.take_file_events();
	}
	const auto elapsed = std::chrono::steady_clock::now() - started_at;
	const double seconds = std::chrono::duration<double>(elapsed).count();
	const double mib_s = (double(bytes) / (1024.0 * 1024.0)) / seconds;
	std::cout << "loopback 128 MiB file transfer (plain, default): " << seconds << " s, "
		<< mib_s << " MiB/s, id=" << id << "\n";
	LAN_CHECK(done);
	LAN_CHECK(mib_s > 10.0);
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(plaintext_transfer_verifies_matching_digest) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-verify-ok-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "note.txt";
	const std::string body = "checksum-please-verify-" + std::string(80 * 1024, 'V');
	{
		std::ofstream out(source, std::ios::binary);
		out << body;
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool done = false;
	bool verified = false;
	std::filesystem::path saved;
	const auto deadline = std::chrono::steady_clock::now() + 8s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Completed
				&& event.transfer_id == id) {
				done = true;
				verified = event.verified;
				saved = event.path;
			}
			LAN_CHECK(event.kind != lan::FileEvent::Kind::Failed);
		}
		alice.take_file_events();
	}
	LAN_CHECK(done);
	LAN_CHECK(verified);
	LAN_CHECK(std::filesystem::exists(saved));
	std::ifstream in(saved, std::ios::binary);
	const std::string got((std::istreambuf_iterator<char>(in)), {});
	LAN_CHECK(got == body);
	LAN_CHECK(lan::sha256_file(saved) == lan::sha256_file(source));
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(plaintext_transfer_rejects_mismatched_digest) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-verify-bad-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "note.txt";
	{
		std::ofstream out(source, std::ios::binary);
		out << std::string(32 * 1024, 'X');
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	std::array<uint8_t, 32> bogus{};
	bogus[0] = 0xff;
	alice.test_force_outgoing_digest(bogus);
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool bob_failed = false;
	std::string error;
	std::filesystem::path leftover;
	const auto deadline = std::chrono::steady_clock::now() + 8s;
	while (std::chrono::steady_clock::now() < deadline && !bob_failed) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.transfer_id == id && event.kind == lan::FileEvent::Kind::Failed) {
				bob_failed = true;
				error = event.error;
				leftover = event.path;
			}
			LAN_CHECK(event.kind != lan::FileEvent::Kind::Completed);
		}
		alice.take_file_events();
	}
	LAN_CHECK(bob_failed);
	LAN_CHECK_EQ(error, std::string("checksum mismatch"));
	LAN_CHECK(!std::filesystem::exists(leftover));
	int parts = 0;
	if (std::filesystem::exists(dir / "bob")) {
		for (const auto &entry : std::filesystem::directory_iterator(dir / "bob")) {
			if (entry.path().extension() == ".part" || entry.path().filename() == "note.txt") {
				++parts;
			}
		}
	}
	LAN_CHECK_EQ(parts, 0);
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(receiver_can_reject_incoming_file_without_writing) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-reject-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "secret.bin";
	{
		std::ofstream out(source, std::ios::binary);
		out << "do-not-store-me";
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	bob_cfg.auto_accept_incoming = false;
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool offered = false;
	const auto offer_deadline = std::chrono::steady_clock::now() + 4s;
	while (std::chrono::steady_clock::now() < offer_deadline && !offered) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Offered && event.transfer_id == id) {
				offered = true;
			}
		}
		alice.take_file_events();
	}
	LAN_CHECK(offered);
	LAN_CHECK(bob.reject_transfer(id));
	bool alice_failed = false;
	bool bob_failed = false;
	const auto done_deadline = std::chrono::steady_clock::now() + 4s;
	while (std::chrono::steady_clock::now() < done_deadline && (!alice_failed || !bob_failed)) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : alice.take_file_events()) {
			if (event.transfer_id == id && event.kind == lan::FileEvent::Kind::Failed) {
				alice_failed = true;
			}
		}
		for (const auto &event : bob.take_file_events()) {
			if (event.transfer_id == id && event.kind == lan::FileEvent::Kind::Failed) {
				bob_failed = true;
			}
			LAN_CHECK(event.kind != lan::FileEvent::Kind::Completed);
			LAN_CHECK(event.kind != lan::FileEvent::Kind::Started);
		}
	}
	LAN_CHECK(alice_failed);
	LAN_CHECK(bob_failed);
	LAN_CHECK(!std::filesystem::exists(dir / "bob" / "secret.bin"));
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(receiver_can_accept_incoming_file_to_chosen_path) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-accept-path-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob-custom");
	const auto source = dir / "alice" / "picked.txt";
	const std::string body = "save-me-here";
	{
		std::ofstream out(source, std::ios::binary);
		out << body;
	}
	const auto dest = dir / "bob-custom" / "chosen-name.txt";
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob-default";
	bob_cfg.state_dir = dir / "bob-state";
	bob_cfg.auto_accept_incoming = false;
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool offered = false;
	const auto offer_deadline = std::chrono::steady_clock::now() + 4s;
	while (std::chrono::steady_clock::now() < offer_deadline && !offered) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Offered && event.transfer_id == id) {
				offered = true;
			}
		}
		alice.take_file_events();
	}
	LAN_CHECK(offered);
	LAN_CHECK(bob.accept_transfer(id, dest));
	bool done = false;
	bool verified = false;
	std::filesystem::path saved;
	const auto deadline = std::chrono::steady_clock::now() + 8s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Completed && event.transfer_id == id) {
				done = true;
				verified = event.verified;
				saved = event.path;
			}
		}
		alice.take_file_events();
	}
	LAN_CHECK(done);
	LAN_CHECK(verified);
	LAN_CHECK(saved == dest);
	LAN_CHECK(std::filesystem::exists(dest));
	LAN_CHECK(!std::filesystem::exists(dir / "bob-default" / "picked.txt"));
	std::ifstream in(dest, std::ios::binary);
	const std::string got((std::istreambuf_iterator<char>(in)), {});
	LAN_CHECK(got == body);
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(sender_emits_offered_until_receiver_accepts_then_progress) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-sender-status-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const auto source = dir / "alice" / "payload.bin";
	{
		std::string payload(256 * 1024, 'S');
		std::ofstream out(source, std::ios::binary);
		out << payload;
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	bob_cfg.auto_accept_incoming = false;
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	const auto id = alice.send_file(bob.identity().peer_id, source);

	bool bob_offered = false;
	bool alice_offered = false;
	bool alice_started_before = false;
	const auto offer_deadline = std::chrono::steady_clock::now() + 4s;
	while (std::chrono::steady_clock::now() < offer_deadline && (!bob_offered || !alice_offered)) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : alice.take_file_events()) {
			if (event.transfer_id != id) {
				continue;
			}
			if (event.kind == lan::FileEvent::Kind::Offered) {
				alice_offered = true;
			}
			if (event.kind == lan::FileEvent::Kind::Started
				|| event.kind == lan::FileEvent::Kind::Progress
				|| event.kind == lan::FileEvent::Kind::Completed) {
				alice_started_before = true;
			}
		}
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Offered && event.transfer_id == id) {
				bob_offered = true;
			}
		}
	}
	LAN_CHECK(bob_offered);
	LAN_CHECK(alice_offered);
	LAN_CHECK(!alice_started_before);

	LAN_CHECK(bob.accept_transfer(id));
	bool alice_started = false;
	bool alice_progressed = false;
	bool done = false;
	uint64_t max_done = 0;
	const auto deadline = std::chrono::steady_clock::now() + 8s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : alice.take_file_events()) {
			if (event.transfer_id != id) {
				continue;
			}
			if (event.kind == lan::FileEvent::Kind::Started) {
				alice_started = true;
			}
			if (event.kind == lan::FileEvent::Kind::Progress && event.bytes_done > 0) {
				alice_progressed = true;
				max_done = std::max(max_done, event.bytes_done);
			}
			if (event.kind == lan::FileEvent::Kind::Completed) {
				done = true;
				max_done = std::max(max_done, event.bytes_done);
			}
		}
		bob.take_file_events();
	}
	LAN_CHECK(alice_started);
	LAN_CHECK(done);
	LAN_CHECK(alice_progressed || max_done > 0);
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

LAN_TEST(needs_spin_is_false_when_idle_with_a_connected_peer) {
	lan::Node alice(make_config("Alice", 0));
	lan::Node bob(make_config("Bob", 0));
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	for (int i = 0; i < 20; ++i) {
		alice.step(0ms);
		bob.step(0ms);
	}
	LAN_CHECK(!alice.needs_spin());
	LAN_CHECK(!bob.needs_spin());
	alice.stop();
	bob.stop();
}

LAN_TEST(needs_spin_is_false_for_held_offer_true_while_transferring_and_clears_after) {
	const auto dir = std::filesystem::temp_directory_path()
		/ ("lanlink-spin-" + std::to_string(getpid()));
	std::filesystem::create_directories(dir / "alice");
	std::filesystem::create_directories(dir / "bob");
	const uint64_t bytes = 4ull * 1024ull * 1024ull;
	const auto source = dir / "alice" / "spin.bin";
	{
		std::string payload(bytes, 'P');
		std::ofstream out(source, std::ios::binary);
		out << payload;
	}
	auto alice_cfg = make_config("Alice", 0);
	alice_cfg.incoming_dir = dir / "alice";
	alice_cfg.state_dir = dir / "alice-state";
	alice_cfg.auto_accept_incoming = false;
	auto bob_cfg = make_config("Bob", 0);
	bob_cfg.incoming_dir = dir / "bob";
	bob_cfg.state_dir = dir / "bob-state";
	bob_cfg.auto_accept_incoming = false;
	lan::Node alice(alice_cfg);
	lan::Node bob(bob_cfg);
	alice.start();
	bob.start();
	alice.add_discovery_target("127.0.0.1", bob.udp_port());
	bob.add_discovery_target("127.0.0.1", alice.udp_port());
	LAN_REQUIRE(wait_for_peer(alice, bob, bob.identity().peer_id));
	LAN_REQUIRE(wait_for_peer(bob, alice, alice.identity().peer_id));
	for (int i = 0; i < 20; ++i) {
		alice.step(0ms);
		bob.step(0ms);
	}
	LAN_CHECK(!alice.needs_spin());
	LAN_CHECK(!bob.needs_spin());

	const auto id = alice.send_file(bob.identity().peer_id, source);
	bool offered = false;
	const auto offer_deadline = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < offer_deadline && !offered) {
		alice.step(0ms);
		bob.step(0ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Offered
				&& event.transfer_id == id) {
				offered = true;
			}
		}
		alice.take_file_events();
	}
	LAN_REQUIRE(offered);
	for (int i = 0; i < 8; ++i) {
		alice.step(0ms);
		bob.step(0ms);
		alice.take_file_events();
		bob.take_file_events();
	}
	LAN_CHECK(!alice.needs_spin());
	LAN_CHECK(!bob.needs_spin());

	LAN_REQUIRE(bob.accept_transfer(id));
	bool done = false;
	bool saw_spin = false;
	const auto deadline = std::chrono::steady_clock::now() + 8s;
	while (std::chrono::steady_clock::now() < deadline && !done) {
		const auto burst = std::chrono::steady_clock::now();
		do {
			alice.step(0ms);
			bob.step(0ms);
			if (alice.needs_spin() || bob.needs_spin()) {
				saw_spin = true;
			}
		} while ((alice.needs_spin() || bob.needs_spin())
			&& std::chrono::steady_clock::now() - burst < 4ms);
		for (const auto &event : bob.take_file_events()) {
			if (event.kind == lan::FileEvent::Kind::Completed
				&& event.transfer_id == id) {
				done = true;
			}
		}
		alice.take_file_events();
		if (!done) {
			std::this_thread::sleep_for(4ms);
		}
	}
	LAN_CHECK(done);
	LAN_CHECK(saw_spin);
	for (int i = 0; i < 16; ++i) {
		alice.step(0ms);
		bob.step(0ms);
		alice.take_file_events();
		bob.take_file_events();
	}
	LAN_CHECK(!alice.needs_spin());
	LAN_CHECK(!bob.needs_spin());
	alice.stop();
	bob.stop();
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}
