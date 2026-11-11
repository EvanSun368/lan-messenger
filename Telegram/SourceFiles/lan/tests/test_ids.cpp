#include "check.h"
#include "identity.h"
#include "lan_ids.h"

#include <set>
#include <string>

LAN_TEST(user_id_from_invalid_peer_is_zero) {
	LAN_CHECK_EQ(lan::user_id_from_peer_id(""), uint64_t(0));
	LAN_CHECK_EQ(lan::user_id_from_peer_id("not-a-peer-id"), uint64_t(0));
	LAN_CHECK_EQ(lan::user_id_from_peer_id("xyz"), uint64_t(0));
	LAN_CHECK(!lan::is_lan_user_id(0));
	LAN_CHECK(!lan::is_lan_user_id(1));
	LAN_CHECK(!lan::is_lan_user_id(123456789ULL));
}

LAN_TEST(user_id_is_stable_marked_and_nonzero) {
	const auto a = lan::create_identity("Alice");
	const auto first = lan::user_id_from_peer_id(a.peer_id);
	const auto again = lan::user_id_from_peer_id(a.peer_id);
	LAN_CHECK(first != 0);
	LAN_CHECK_EQ(first, again);
	LAN_CHECK(lan::is_lan_user_id(first));
	LAN_CHECK_EQ(first & lan::kLanUserIdMarker, lan::kLanUserIdMarker);
	LAN_CHECK_EQ(first >> 48, uint64_t(0));
}

LAN_TEST(distinct_peer_ids_map_to_distinct_user_ids) {
	std::set<uint64_t> ids;
	for (int i = 0; i < 32; ++i) {
		const auto identity = lan::create_identity("Peer-" + std::to_string(i));
		const auto id = lan::user_id_from_peer_id(identity.peer_id);
		LAN_CHECK(lan::is_lan_user_id(id));
		LAN_CHECK(ids.insert(id).second);
	}
}
