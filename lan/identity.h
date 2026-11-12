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
	// Binding to the machine that last opened this folder. Empty on a
	// legacy identity that has not been stamped yet.
	std::string machine_fingerprint;
};

bool is_valid_peer_id(std::string_view peer_id);
bool is_valid_display_name(std::string_view name);
bool is_valid_machine_fingerprint(std::string_view fingerprint);
// Trims ASCII whitespace. Returns empty string if the result is invalid.
std::string normalize_display_name(std::string_view name);

// Joins an OS machine-id and a volume/device id. Returns empty if both
// parts are empty after sanitizing.
std::string make_machine_fingerprint(
	std::string_view os_machine_id,
	std::string_view volume_id);
// Reads the current OS machine-id and the volume/device id of dir.
std::string current_machine_fingerprint(const std::filesystem::path &dir);

// Hostname of this machine, or empty if it cannot be read.
std::string current_hostname();
// Uses hostname when it normalizes to a valid display name (long names
// are truncated to 64). Otherwise "LanLink-xxxx" derived from fallback_token.
std::string default_display_name_from(
	std::string_view hostname,
	std::string_view fallback_token);

// Creates a new identity with a fresh peer_id. Throws if the name is invalid.
Identity create_identity(std::string_view display_name);

// Stores files in dir: peer_id, display_name, and machine_fingerprint
// when the fingerprint is non-empty.
void save_identity(const std::filesystem::path &dir, const Identity &identity);
Identity load_identity(const std::filesystem::path &dir);

// Loads an existing identity, or creates and saves one.
// If a stored identity exists, the peer_id is kept and display_name is
// updated when the provided name is non-empty and different — unless
// machine_fingerprint is supplied and differs from the stored value, in
// which case a new peer_id is created, the name is replaced, and a bound
// tde2e key file is discarded. A legacy identity with no fingerprint is
// stamped with the provided fingerprint and keeps its peer_id.
Identity load_or_create_identity(
	const std::filesystem::path &dir,
	std::string_view display_name);
Identity load_or_create_identity(
	const std::filesystem::path &dir,
	std::string_view display_name,
	std::string_view machine_fingerprint);

} // namespace lan
