#include "group.h"

#include "identity.h"
#include "protocol.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace lan {
namespace {

void append_u16be(std::vector<uint8_t> &out, uint16_t value) {
	out.push_back(static_cast<uint8_t>(value >> 8));
	out.push_back(static_cast<uint8_t>(value));
}

void append_u32be(std::vector<uint8_t> &out, uint32_t value) {
	out.push_back(static_cast<uint8_t>(value >> 24));
	out.push_back(static_cast<uint8_t>(value >> 16));
	out.push_back(static_cast<uint8_t>(value >> 8));
	out.push_back(static_cast<uint8_t>(value));
}

bool read_u16be(const uint8_t *&cursor, const uint8_t *end, uint16_t &value) {
	if (end - cursor < 2) {
		return false;
	}
	value = static_cast<uint16_t>((cursor[0] << 8) | cursor[1]);
	cursor += 2;
	return true;
}

bool read_u32be(const uint8_t *&cursor, const uint8_t *end, uint32_t &value) {
	if (end - cursor < 4) {
		return false;
	}
	value = (static_cast<uint32_t>(cursor[0]) << 24)
		| (static_cast<uint32_t>(cursor[1]) << 16)
		| (static_cast<uint32_t>(cursor[2]) << 8)
		| static_cast<uint32_t>(cursor[3]);
	cursor += 4;
	return true;
}

bool read_counted(const uint8_t *&cursor, const uint8_t *end, uint16_t max, std::string &out) {
	uint16_t len = 0;
	if (!read_u16be(cursor, end, len) || len > max) {
		return false;
	}
	if (static_cast<size_t>(end - cursor) < len) {
		return false;
	}
	out.assign(reinterpret_cast<const char *>(cursor), len);
	cursor += len;
	return true;
}

void append_counted(std::vector<uint8_t> &out, std::string_view value) {
	append_u16be(out, static_cast<uint16_t>(value.size()));
	out.insert(out.end(), value.begin(), value.end());
}

void sort_unique(std::vector<std::string> &ids) {
	std::sort(ids.begin(), ids.end());
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

std::filesystem::path group_path(
	const std::filesystem::path &dir,
	std::string_view group_id) {
	return dir / (std::string(group_id) + ".group");
}

} // namespace

bool is_valid_group_name(std::string_view name) {
	return is_valid_display_name(name);
}

std::string normalize_group_name(std::string_view name) {
	return normalize_display_name(name);
}

bool group_has_member(const Group &group, std::string_view peer_id) {
	return std::binary_search(
		group.member_ids.begin(),
		group.member_ids.end(),
		peer_id);
}

bool group_add_member(Group &group, std::string peer_id) {
	if (!is_valid_peer_id(peer_id) || group_has_member(group, peer_id)) {
		return false;
	}
	group.member_ids.push_back(std::move(peer_id));
	sort_unique(group.member_ids);
	return true;
}

bool group_remove_member(Group &group, std::string_view peer_id) {
	if (peer_id == group.creator_id) {
		return false;
	}
	const auto it = std::find(group.member_ids.begin(), group.member_ids.end(), peer_id);
	if (it == group.member_ids.end()) {
		return false;
	}
	group.member_ids.erase(it);
	return true;
}

Group make_group(
		std::string_view name,
		std::string creator_id,
		std::vector<std::string> member_ids) {
	const auto normalized = normalize_group_name(name);
	if (normalized.empty() || !is_valid_peer_id(creator_id)) {
		throw std::invalid_argument("invalid group");
	}
	Group group;
	group.group_id = random_hex_id();
	group.name = normalized;
	group.creator_id = std::move(creator_id);
	group.member_ids = std::move(member_ids);
	group_add_member(group, group.creator_id);
	sort_unique(group.member_ids);
	for (const auto &id : group.member_ids) {
		if (!is_valid_peer_id(id)) {
			throw std::invalid_argument("invalid group member");
		}
	}
	return group;
}

std::vector<uint8_t> encode_group_roster(const GroupRosterPlain &roster) {
	if (!is_valid_display_name(roster.name)
		|| roster.name.size() > 255
		|| !is_valid_peer_id(roster.creator_id)
		|| roster.member_ids.size() > 1024) {
		throw std::invalid_argument("invalid group roster");
	}
	std::vector<uint8_t> out;
	append_counted(out, roster.name);
	const auto creator = from_hex(roster.creator_id);
	out.insert(out.end(), creator->begin(), creator->end());
	append_u32be(out, static_cast<uint32_t>(roster.member_ids.size()));
	for (const auto &id : roster.member_ids) {
		const auto raw = from_hex(id);
		if (!raw || raw->size() != 16) {
			throw std::invalid_argument("invalid group member");
		}
		out.insert(out.end(), raw->begin(), raw->end());
	}
	return out;
}

std::optional<GroupRosterPlain> decode_group_roster(
		const uint8_t *data,
		size_t size) {
	if (data == nullptr) {
		return std::nullopt;
	}
	const uint8_t *cursor = data;
	const uint8_t *end = data + size;
	GroupRosterPlain roster;
	uint32_t count = 0;
	if (!read_counted(cursor, end, 255, roster.name)
		|| !is_valid_display_name(roster.name)
		|| static_cast<size_t>(end - cursor) < 16) {
		return std::nullopt;
	}
	roster.creator_id = to_hex(cursor, 16);
	cursor += 16;
	if (!is_valid_peer_id(roster.creator_id)
		|| !read_u32be(cursor, end, count)
		|| count > 1024
		|| static_cast<size_t>(end - cursor) != size_t(count) * 16) {
		return std::nullopt;
	}
	roster.member_ids.reserve(count);
	for (uint32_t i = 0; i < count; ++i) {
		auto id = to_hex(cursor, 16);
		cursor += 16;
		if (!is_valid_peer_id(id)) {
			return std::nullopt;
		}
		roster.member_ids.push_back(std::move(id));
	}
	sort_unique(roster.member_ids);
	if (!std::binary_search(
			roster.member_ids.begin(),
			roster.member_ids.end(),
			roster.creator_id)) {
		roster.member_ids.push_back(roster.creator_id);
		sort_unique(roster.member_ids);
	}
	return roster;
}

Group group_from_roster(const std::string &group_id, const GroupRosterPlain &roster) {
	if (!is_valid_peer_id(group_id)) {
		throw std::invalid_argument("invalid group id");
	}
	Group group;
	group.group_id = group_id;
	group.name = roster.name;
	group.creator_id = roster.creator_id;
	group.member_ids = roster.member_ids;
	sort_unique(group.member_ids);
	return group;
}

GroupRosterPlain roster_from_group(const Group &group) {
	return GroupRosterPlain{group.name, group.creator_id, group.member_ids};
}

void save_group(const std::filesystem::path &dir, const Group &group) {
	if (!is_valid_peer_id(group.group_id)) {
		throw std::invalid_argument("invalid group id");
	}
	std::filesystem::create_directories(dir);
	const auto encoded = encode_group_roster(roster_from_group(group));
	const auto dest = group_path(dir, group.group_id);
	const auto tmp = dest.string() + ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		out.write(
			reinterpret_cast<const char *>(encoded.data()),
			static_cast<std::streamsize>(encoded.size()));
		if (!out) {
			throw std::runtime_error("unable to write group");
		}
	}
	std::filesystem::rename(tmp, dest);
}

std::optional<Group> load_group(
		const std::filesystem::path &dir,
		std::string_view group_id) {
	std::ifstream in(group_path(dir, group_id), std::ios::binary);
	if (!in) {
		return std::nullopt;
	}
	std::vector<uint8_t> data(
		(std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
	const auto roster = decode_group_roster(data.data(), data.size());
	if (!roster || !is_valid_peer_id(group_id)) {
		return std::nullopt;
	}
	return group_from_roster(std::string(group_id), *roster);
}

void remove_group(
		const std::filesystem::path &dir,
		std::string_view group_id) {
	std::error_code ec;
	std::filesystem::remove(group_path(dir, group_id), ec);
}

std::vector<Group> load_all_groups(const std::filesystem::path &dir) {
	std::vector<Group> out;
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec)) {
		return out;
	}
	for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
		if (ec || !entry.is_regular_file(ec) || entry.path().extension() != ".group") {
			continue;
		}
		const auto id = entry.path().stem().string();
		auto group = load_group(dir, id);
		if (group) {
			out.push_back(std::move(*group));
		}
	}
	return out;
}

} // namespace lan
