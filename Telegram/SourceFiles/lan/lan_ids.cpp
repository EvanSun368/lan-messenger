#include "lan_ids.h"

#include "identity.h"

namespace lan {
namespace {

constexpr uint64_t kPayloadMask = 0x3FFFFFFFFFFFULL; // 46 bits below the marker

} // namespace

uint64_t user_id_from_peer_id(std::string_view peer_id) {
	if (!is_valid_peer_id(peer_id)) {
		return 0;
	}
	// FNV-1a 64 over the canonical hex string.
	uint64_t hash = 14695981039346656037ULL;
	for (const unsigned char c : peer_id) {
		hash ^= c;
		hash *= 1099511628211ULL;
	}
	auto payload = hash & kPayloadMask;
	if (payload == 0) {
		payload = 1;
	}
	return payload | kLanUserIdMarker;
}

bool is_lan_user_id(uint64_t user_id) {
	return (user_id & kLanUserIdMarker) == kLanUserIdMarker
		&& (user_id & kPayloadMask) != 0
		&& (user_id >> 48) == 0;
}

} // namespace lan
