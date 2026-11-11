#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lan {

// Official tde2e public keys are 32 raw bytes (see tde2e_api::key_to_public_key).
constexpr size_t kE2EPublicKeySize = 32;

[[nodiscard]] bool is_valid_public_key(const std::vector<uint8_t> &key);

// Thin wrapper around the official tde2e one-shot ECDH envelope:
//   key_generate_private_key / key_from_words / key_to_words
//   key_to_public_key / key_from_public_key
//   key_from_ecdh
//   encrypt_message_for_one / decrypt_message_for_one
// No homemade cipher lives here.
class E2EKey {
public:
	[[nodiscard]] static E2EKey generate();
	[[nodiscard]] static E2EKey from_words(std::string_view words);

	E2EKey(E2EKey &&other) noexcept;
	E2EKey &operator=(E2EKey &&other) noexcept;
	E2EKey(const E2EKey &) = delete;
	E2EKey &operator=(const E2EKey &) = delete;
	~E2EKey();

	[[nodiscard]] std::string words() const;
	[[nodiscard]] const std::vector<uint8_t> &public_key() const;

	// Seals plaintext to a peer's official 32-byte public key.
	[[nodiscard]] std::vector<uint8_t> encrypt_for(
		const std::vector<uint8_t> &peer_public_key,
		std::string_view plaintext) const;

	// Opens a ciphertext produced by encrypt_for with this key's public key.
	[[nodiscard]] std::optional<std::string> decrypt_from(
		const std::vector<uint8_t> &peer_public_key,
		std::string_view ciphertext) const;

private:
	E2EKey(std::int64_t private_key_id, std::vector<uint8_t> public_key);

	std::int64_t private_key_id_ = 0;
	std::vector<uint8_t> public_key_;
};

// Persist the official mnemonic export next to the LAN identity.
void save_e2e_key(const std::filesystem::path &dir, const E2EKey &key);
[[nodiscard]] E2EKey load_e2e_key(const std::filesystem::path &dir);
[[nodiscard]] E2EKey load_or_create_e2e_key(const std::filesystem::path &dir);

} // namespace lan
