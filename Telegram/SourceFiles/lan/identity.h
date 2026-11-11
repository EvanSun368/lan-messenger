#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace lan {

// Local, offline identity. No phone number and no Telegram account.
struct Identity {
	// 32 lowercase hex characters (16 random bytes).
	std::string peer_id;
	// 1..64 characters, no control characters.
	std::string display_name;
};

bool is_valid_peer_id(std::string_view peer_id);
bool is_valid_display_name(std::string_view name);
// Trims ASCII whitespace. Returns empty string if the result is invalid.
std::string normalize_display_name(std::string_view name);

// Creates a new identity with a fresh peer_id. Throws if the name is invalid.
Identity create_identity(std::string_view display_name);

// Stores two files in dir: peer_id and display_name.
void save_identity(const std::filesystem::path &dir, const Identity &identity);
Identity load_identity(const std::filesystem::path &dir);

// Loads an existing identity, or creates and saves one.
// If a stored identity exists, the peer_id is kept and display_name is
// updated when the provided name is non-empty and different.
Identity load_or_create_identity(
	const std::filesystem::path &dir,
	std::string_view display_name);

} // namespace lan
