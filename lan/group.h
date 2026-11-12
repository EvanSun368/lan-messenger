#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lan {

// A serverless LAN group. Membership is trusted-LAN: anyone on the
// local network who holds the group id can be invited. Payloads are
// still sealed per recipient with official tde2e; there is no
// plaintext fallback.
struct Group {
	std::string group_id; // 32 hex
	std::string name;
	std::string creator_id; // 32 hex
	// Includes the creator. Sorted and unique.
	std::vector<std::string> member_ids;
};

struct GroupRosterPlain {
	std::string name;
	std::string creator_id;
	std::vector<std::string> member_ids;
};

bool is_valid_group_name(std::string_view name);
// Trims and accepts the same rules as a display name. Empty if invalid.
std::string normalize_group_name(std::string_view name);

bool group_has_member(const Group &group, std::string_view peer_id);
// Returns true when the member was inserted.
bool group_add_member(Group &group, std::string peer_id);
// Returns true when the member was present and removed. The creator
// cannot be removed.
bool group_remove_member(Group &group, std::string_view peer_id);

Group make_group(
	std::string_view name,
	std::string creator_id,
	std::vector<std::string> member_ids = {});

std::vector<uint8_t> encode_group_roster(const GroupRosterPlain &roster);
std::optional<GroupRosterPlain> decode_group_roster(
	const uint8_t *data,
	size_t size);

Group group_from_roster(const std::string &group_id, const GroupRosterPlain &roster);
GroupRosterPlain roster_from_group(const Group &group);

void save_group(const std::filesystem::path &dir, const Group &group);
std::optional<Group> load_group(
	const std::filesystem::path &dir,
	std::string_view group_id);
void remove_group(
	const std::filesystem::path &dir,
	std::string_view group_id);
std::vector<Group> load_all_groups(const std::filesystem::path &dir);

} // namespace lan
