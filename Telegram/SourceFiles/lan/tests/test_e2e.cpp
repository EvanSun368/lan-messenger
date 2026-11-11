#include "check.h"
#include "e2e.h"
#include "protocol.h"

#include <filesystem>
#include <string>
#include <unistd.h>

namespace {

std::filesystem::path make_temp_dir() {
	auto path = std::filesystem::temp_directory_path()
		/ ("lanlink-e2e-" + std::to_string(getpid()) + "-" + std::to_string(rand()));
	std::filesystem::create_directories(path);
	return path;
}

} // namespace

LAN_TEST(e2e_public_key_is_32_official_bytes) {
	const auto key = lan::E2EKey::generate();
	LAN_CHECK(lan::is_valid_public_key(key.public_key()));
	LAN_CHECK_EQ(key.public_key().size(), lan::kE2EPublicKeySize);
	LAN_CHECK(!lan::is_valid_public_key({}));
	LAN_CHECK(!lan::is_valid_public_key(std::vector<uint8_t>(31, 1)));
}

LAN_TEST(e2e_encrypt_decrypt_roundtrip_is_not_plaintext) {
	const auto alice = lan::E2EKey::generate();
	const auto bob = lan::E2EKey::generate();
	const std::string plain = "hello from alice, not for the wire";
	const auto cipher = alice.encrypt_for(bob.public_key(), plain);
	LAN_CHECK(!cipher.empty());
	const std::string cipher_view(cipher.begin(), cipher.end());
	LAN_CHECK(cipher_view.find(plain) == std::string::npos);

	const auto opened = bob.decrypt_from(
		alice.public_key(),
		std::string_view(cipher_view));
	LAN_REQUIRE(opened.has_value());
	LAN_CHECK_EQ(*opened, plain);
}

LAN_TEST(e2e_decrypt_fails_closed_for_the_wrong_key) {
	const auto alice = lan::E2EKey::generate();
	const auto bob = lan::E2EKey::generate();
	const auto eve = lan::E2EKey::generate();
	const auto cipher = alice.encrypt_for(bob.public_key(), "secret");
	const std::string cipher_view(cipher.begin(), cipher.end());
	LAN_CHECK(!eve.decrypt_from(
		alice.public_key(),
		std::string_view(cipher_view)).has_value());
	LAN_CHECK(!bob.decrypt_from(
		eve.public_key(),
		std::string_view(cipher_view)).has_value());
}

LAN_TEST(e2e_words_roundtrip_keeps_the_same_public_key) {
	const auto original = lan::E2EKey::generate();
	const auto words = original.words();
	LAN_CHECK(!words.empty());
	auto restored = lan::E2EKey::from_words(words);
	LAN_CHECK(restored.public_key() == original.public_key());
	const auto cipher = original.encrypt_for(restored.public_key(), "loop");
	const std::string cipher_view(cipher.begin(), cipher.end());
	// Encrypting to our own public key should open with the restored key.
	const auto opened = restored.decrypt_from(
		original.public_key(),
		std::string_view(cipher_view));
	LAN_REQUIRE(opened.has_value());
	LAN_CHECK_EQ(*opened, std::string("loop"));
}

LAN_TEST(e2e_key_persists_next_to_identity) {
	const auto dir = make_temp_dir();
	const auto first = lan::load_or_create_e2e_key(dir);
	const auto again = lan::load_or_create_e2e_key(dir);
	LAN_CHECK(first.public_key() == again.public_key());
	std::filesystem::remove_all(dir);
}
