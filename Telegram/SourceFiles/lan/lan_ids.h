#pragma once

#include <cstdint>
#include <string_view>

namespace lan {

// Bit 46 marks a synthetic UserId derived from a LAN peer_id.
// It stays inside tdesktop's 48-bit peer payload (kChatTypeMask).
inline constexpr uint64_t kLanUserIdMarker = 0x400000000000ULL;

// Deterministic, stable mapping used to fabricate a local UserId.
// Returns 0 if peer_id is not a valid 32-character hex id.
[[nodiscard]] uint64_t user_id_from_peer_id(std::string_view peer_id);

[[nodiscard]] bool is_lan_user_id(uint64_t user_id);

} // namespace lan
