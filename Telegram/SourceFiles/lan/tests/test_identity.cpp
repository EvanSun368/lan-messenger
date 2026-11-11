#include "check.h"
#include "identity.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::filesystem::path make_temp_dir() {
	auto path = std::filesystem::temp_directory_path()
		/ ("lanlink-id-" + std::to_string(getpid()) + "-" + std::to_string(rand()));
	std::filesystem::create_directories(path);
	return path;
}

} // namespace

LAN_TEST(normalize_trims_and_rejects_empty) {
	LAN_CHECK_EQ(lan::normalize_display_name("  Alice  "), std::string("Alice"));
	LAN_CHECK(lan::normalize_display_name("   ").empty());
	LAN_CHECK(lan::normalize_display_name("").empty());
	LAN_CHECK(lan::normalize_display_name("bad\nname").empty());
	LAN_CHECK(lan::normalize_display_name(std::string(65, 'a')).empty());
	LAN_CHECK_EQ(lan::normalize_display_name(std::string(64, 'a')), std::string(64, 'a'));
}

LAN_TEST(create_identity_assigns_valid_id) {
	const auto identity = lan::create_identity("  Bob  ");
	LAN_CHECK(lan::is_valid_peer_id(identity.peer_id));
	LAN_CHECK_EQ(identity.peer_id.size(), size_t(32));
	LAN_CHECK_EQ(identity.display_name, std::string("Bob"));
	const auto other = lan::create_identity("Bob");
	LAN_CHECK(identity.peer_id != other.peer_id);
}

LAN_TEST(create_identity_rejects_invalid_name) {
	bool threw = false;
	try {
		lan::create_identity("");
	} catch (const std::exception &) {
		threw = true;
	}
	LAN_CHECK(threw);
}

LAN_TEST(save_and_load_roundtrip) {
	const auto dir = make_temp_dir();
	const auto original = lan::create_identity("车间-1");
	lan::save_identity(dir, original);
	const auto loaded = lan::load_identity(dir);
	LAN_CHECK_EQ(loaded.peer_id, original.peer_id);
	LAN_CHECK_EQ(loaded.display_name, original.display_name);
	std::filesystem::remove_all(dir);
}

LAN_TEST(load_or_create_keeps_peer_id_and_can_rename) {
	const auto dir = make_temp_dir();
	const auto first = lan::load_or_create_identity(dir, "Alice");
	const auto again = lan::load_or_create_identity(dir, "Alice");
	LAN_CHECK_EQ(first.peer_id, again.peer_id);
	LAN_CHECK_EQ(again.display_name, std::string("Alice"));
	const auto renamed = lan::load_or_create_identity(dir, "Alice-2");
	LAN_CHECK_EQ(renamed.peer_id, first.peer_id);
	LAN_CHECK_EQ(renamed.display_name, std::string("Alice-2"));
	std::filesystem::remove_all(dir);
}
