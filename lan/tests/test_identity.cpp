#include "check.h"
#include "identity.h"
#include "e2e.h"

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

LAN_TEST(make_machine_fingerprint_joins_os_and_volume) {
	LAN_CHECK(lan::make_machine_fingerprint("", "").empty());
	LAN_CHECK_EQ(
		lan::make_machine_fingerprint("abc123", "8:3"),
		std::string("os=abc123;vol=8:3"));
	LAN_CHECK(lan::is_valid_machine_fingerprint(
		lan::make_machine_fingerprint("abc123", "8:3")));
	LAN_CHECK(lan::make_machine_fingerprint("bad\nid", "vol").find('\n')
		== std::string::npos);
}

LAN_TEST(default_display_name_uses_hostname_or_stable_fallback) {
	LAN_CHECK_EQ(
		lan::default_display_name_from("  Shop-PC  ", "token"),
		std::string("Shop-PC"));
	LAN_CHECK_EQ(
		lan::default_display_name_from(std::string(65, 'a'), "token"),
		std::string(64, 'a'));
	LAN_CHECK(lan::default_display_name_from("", "token-a")
		!= lan::default_display_name_from("", "token-b"));
	LAN_CHECK_EQ(
		lan::default_display_name_from("", "same"),
		lan::default_display_name_from("   ", "same"));
	const auto fallback = lan::default_display_name_from("", "same");
	LAN_CHECK(fallback.size() >= 8);
	LAN_CHECK(fallback.rfind("LanLink-", 0) == 0);
	LAN_CHECK(lan::is_valid_display_name(fallback));
}

LAN_TEST(load_or_create_keeps_peer_id_for_same_fingerprint) {
	const auto dir = make_temp_dir();
	const auto first = lan::load_or_create_identity(dir, "Alice", "fp-one");
	LAN_CHECK_EQ(first.machine_fingerprint, std::string("fp-one"));
	const auto again = lan::load_or_create_identity(dir, "Alice", "fp-one");
	LAN_CHECK_EQ(first.peer_id, again.peer_id);
	LAN_CHECK_EQ(again.display_name, std::string("Alice"));
	LAN_CHECK_EQ(again.machine_fingerprint, std::string("fp-one"));
	std::filesystem::remove_all(dir);
}

LAN_TEST(load_or_create_rotates_peer_id_for_different_fingerprint) {
	const auto dir = make_temp_dir();
	const auto first = lan::load_or_create_identity(dir, "Old-PC", "fp-one");
	const auto rotated = lan::load_or_create_identity(dir, "New-PC", "fp-two");
	LAN_CHECK(rotated.peer_id != first.peer_id);
	LAN_CHECK(lan::is_valid_peer_id(rotated.peer_id));
	LAN_CHECK_EQ(rotated.display_name, std::string("New-PC"));
	LAN_CHECK_EQ(rotated.machine_fingerprint, std::string("fp-two"));
	const auto again = lan::load_or_create_identity(dir, "New-PC", "fp-two");
	LAN_CHECK_EQ(again.peer_id, rotated.peer_id);
	std::filesystem::remove_all(dir);
}

LAN_TEST(legacy_identity_without_fingerprint_is_stamped_not_rotated) {
	const auto dir = make_temp_dir();
	const auto original = lan::create_identity("Legacy-PC");
	lan::save_identity(dir, original);
	LAN_CHECK(!std::filesystem::exists(dir / "machine_fingerprint"));
	const auto stamped = lan::load_or_create_identity(
		dir,
		"Should-Not-Rename-On-Stamp",
		"fp-now");
	LAN_CHECK_EQ(stamped.peer_id, original.peer_id);
	LAN_CHECK_EQ(stamped.display_name, std::string("Legacy-PC"));
	LAN_CHECK_EQ(stamped.machine_fingerprint, std::string("fp-now"));
	const auto loaded = lan::load_identity(dir);
	LAN_CHECK_EQ(loaded.peer_id, original.peer_id);
	LAN_CHECK_EQ(loaded.machine_fingerprint, std::string("fp-now"));
	std::filesystem::remove_all(dir);
}

LAN_TEST(rotation_discards_bound_e2e_key) {
	const auto dir = make_temp_dir();
	lan::load_or_create_identity(dir, "Old-PC", "fp-one");
	const auto first_key = lan::load_or_create_e2e_key(dir);
	LAN_CHECK(std::filesystem::exists(dir / "e2e_words"));
	lan::load_or_create_identity(dir, "New-PC", "fp-two");
	LAN_CHECK(!std::filesystem::exists(dir / "e2e_words"));
	const auto second_key = lan::load_or_create_e2e_key(dir);
	LAN_CHECK(first_key.public_key() != second_key.public_key());
	std::filesystem::remove_all(dir);
}

LAN_TEST(current_machine_fingerprint_is_stable_on_this_host) {
	const auto dir = make_temp_dir();
	const auto first = lan::current_machine_fingerprint(dir);
	const auto again = lan::current_machine_fingerprint(dir);
	LAN_CHECK(!first.empty());
	LAN_CHECK(lan::is_valid_machine_fingerprint(first));
	LAN_CHECK_EQ(first, again);
	std::filesystem::remove_all(dir);
}
