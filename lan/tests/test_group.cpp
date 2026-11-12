#include "check.h"
#include "group.h"
#include "identity.h"
#include "protocol.h"

#include <algorithm>
#include <filesystem>
#include <unistd.h>

namespace {

std::filesystem::path make_temp_dir() {
	auto path = std::filesystem::temp_directory_path()
		/ ("lanlink-group-" + std::to_string(getpid()) + "-" + std::to_string(rand()));
	std::filesystem::create_directories(path);
	return path;
}

} // namespace

LAN_TEST(make_group_includes_creator_and_rejects_bad_names) {
	const auto alice = lan::random_hex_id();
	const auto bob = lan::random_hex_id();
	const auto group = lan::make_group("  车间群  ", alice, {bob});
	LAN_CHECK(lan::is_valid_peer_id(group.group_id));
	LAN_CHECK_EQ(group.name, std::string("车间群"));
	LAN_CHECK_EQ(group.creator_id, alice);
	LAN_CHECK(lan::group_has_member(group, alice));
	LAN_CHECK(lan::group_has_member(group, bob));
	LAN_CHECK_EQ(group.member_ids.size(), size_t(2));
	bool threw = false;
	try {
		lan::make_group("", alice);
	} catch (const std::exception &) {
		threw = true;
	}
	LAN_CHECK(threw);
}

LAN_TEST(group_add_and_remove_members) {
	const auto alice = lan::random_hex_id();
	const auto bob = lan::random_hex_id();
	auto group = lan::make_group("Team", alice);
	LAN_CHECK(lan::group_add_member(group, bob));
	LAN_CHECK(!lan::group_add_member(group, bob));
	LAN_CHECK(!lan::group_remove_member(group, alice));
	LAN_CHECK(lan::group_has_member(group, alice));
	LAN_CHECK(lan::group_remove_member(group, bob));
	LAN_CHECK(!lan::group_has_member(group, bob));
}

LAN_TEST(group_roster_roundtrip_and_persist) {
	const auto dir = make_temp_dir();
	const auto alice = lan::random_hex_id();
	const auto bob = lan::random_hex_id();
	auto group = lan::make_group("Team", alice, {bob});
	const auto encoded = lan::encode_group_roster(lan::roster_from_group(group));
	const auto decoded = lan::decode_group_roster(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->name, std::string("Team"));
	LAN_CHECK_EQ(decoded->creator_id, alice);
	LAN_CHECK_EQ(decoded->member_ids.size(), size_t(2));
	lan::save_group(dir, group);
	const auto loaded = lan::load_group(dir, group.group_id);
	LAN_REQUIRE(loaded.has_value());
	LAN_CHECK_EQ(loaded->name, group.name);
	LAN_CHECK_EQ(loaded->creator_id, alice);
	LAN_CHECK(lan::group_has_member(*loaded, bob));
	const auto all = lan::load_all_groups(dir);
	LAN_CHECK_EQ(all.size(), size_t(1));
	lan::remove_group(dir, group.group_id);
	LAN_CHECK(!lan::load_group(dir, group.group_id).has_value());
	std::filesystem::remove_all(dir);
}
