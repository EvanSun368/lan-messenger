#include "e2e.h"

#include <fstream>
#include <stdexcept>

#include <td/e2e/e2e_api.h>

namespace lan {
namespace {

tde2e_api::Slice slice_bytes(const std::vector<uint8_t> &data) {
	return {
		reinterpret_cast<const char *>(data.data()),
		data.size(),
	};
}

tde2e_api::Slice slice_view(std::string_view data) {
	return { data.data(), data.size() };
}

void destroy_key(std::int64_t id) {
	if (id) {
		tde2e_api::key_destroy(id);
	}
}

std::int64_t import_peer_public(const std::vector<uint8_t> &peer_public_key) {
	if (!is_valid_public_key(peer_public_key)) {
		return 0;
	}
	const auto peer = tde2e_api::key_from_public_key(slice_bytes(peer_public_key));
	if (!peer.is_ok()) {
		return 0;
	}
	return peer.value();
}

void write_file(const std::filesystem::path &path, std::string_view value) {
	std::ofstream out(path, std::ios::trunc);
	if (!out) {
		throw std::runtime_error("unable to write " + path.string());
	}
	out << value;
	if (!value.empty() && value.back() != '\n') {
		out << '\n';
	}
}

std::string read_file(const std::filesystem::path &path) {
	std::ifstream in(path);
	if (!in) {
		throw std::runtime_error("unable to read " + path.string());
	}
	std::string text(
		(std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
		text.pop_back();
	}
	return text;
}

} // namespace

bool is_valid_public_key(const std::vector<uint8_t> &key) {
	return key.size() == kE2EPublicKeySize;
}

E2EKey::E2EKey(std::int64_t private_key_id, std::vector<uint8_t> public_key)
: private_key_id_(private_key_id)
, public_key_(std::move(public_key)) {
}

E2EKey E2EKey::generate() {
	const auto id = tde2e_api::key_generate_private_key();
	if (!id.is_ok()) {
		throw std::runtime_error("tde2e failed to generate a private key");
	}
	const auto published = tde2e_api::key_to_public_key(id.value());
	if (!published.is_ok()
		|| published.value().size() != kE2EPublicKeySize) {
		destroy_key(id.value());
		throw std::runtime_error("tde2e failed to export a public key");
	}
	std::vector<uint8_t> bytes(
		published.value().begin(),
		published.value().end());
	return E2EKey(id.value(), std::move(bytes));
}

E2EKey E2EKey::from_words(std::string_view words) {
	const auto id = tde2e_api::key_from_words(slice_view(words));
	if (!id.is_ok()) {
		throw std::runtime_error("tde2e failed to import a private key");
	}
	const auto published = tde2e_api::key_to_public_key(id.value());
	if (!published.is_ok()
		|| published.value().size() != kE2EPublicKeySize) {
		destroy_key(id.value());
		throw std::runtime_error("tde2e failed to export a public key");
	}
	std::vector<uint8_t> bytes(
		published.value().begin(),
		published.value().end());
	return E2EKey(id.value(), std::move(bytes));
}

E2EKey::E2EKey(E2EKey &&other) noexcept
: private_key_id_(other.private_key_id_)
, public_key_(std::move(other.public_key_)) {
	other.private_key_id_ = 0;
}

E2EKey &E2EKey::operator=(E2EKey &&other) noexcept {
	if (this != &other) {
		destroy_key(private_key_id_);
		private_key_id_ = other.private_key_id_;
		public_key_ = std::move(other.public_key_);
		other.private_key_id_ = 0;
	}
	return *this;
}

E2EKey::~E2EKey() {
	destroy_key(private_key_id_);
	private_key_id_ = 0;
}

std::string E2EKey::words() const {
	const auto words = tde2e_api::key_to_words(private_key_id_);
	if (!words.is_ok() || words.value().empty()) {
		throw std::runtime_error("tde2e failed to export key words");
	}
	return words.value();
}

const std::vector<uint8_t> &E2EKey::public_key() const {
	return public_key_;
}

std::vector<uint8_t> E2EKey::encrypt_for(
		const std::vector<uint8_t> &peer_public_key,
		std::string_view plaintext) const {
	const auto peer_id = import_peer_public(peer_public_key);
	if (!peer_id || !private_key_id_) {
		throw std::runtime_error("tde2e missing a peer public key");
	}
	const auto shared = tde2e_api::key_from_ecdh(private_key_id_, peer_id);
	destroy_key(peer_id);
	if (!shared.is_ok()) {
		throw std::runtime_error("tde2e ECDH failed");
	}
	const auto shared_id = shared.value();
	const auto encrypted = tde2e_api::encrypt_message_for_one(
		shared_id,
		slice_view(plaintext));
	destroy_key(shared_id);
	if (!encrypted.is_ok()) {
		throw std::runtime_error("tde2e encrypt_message_for_one failed");
	}
	return {
		encrypted.value().begin(),
		encrypted.value().end(),
	};
}

std::optional<std::string> E2EKey::decrypt_from(
		const std::vector<uint8_t> &peer_public_key,
		std::string_view ciphertext) const {
	const auto peer_id = import_peer_public(peer_public_key);
	if (!peer_id || !private_key_id_) {
		return std::nullopt;
	}
	const auto shared = tde2e_api::key_from_ecdh(private_key_id_, peer_id);
	destroy_key(peer_id);
	if (!shared.is_ok()) {
		return std::nullopt;
	}
	const auto shared_id = shared.value();
	const auto decrypted = tde2e_api::decrypt_message_for_one(
		shared_id,
		slice_view(ciphertext));
	destroy_key(shared_id);
	if (!decrypted.is_ok()) {
		return std::nullopt;
	}
	return decrypted.value();
}

void save_e2e_key(const std::filesystem::path &dir, const E2EKey &key) {
	std::filesystem::create_directories(dir);
	write_file(dir / "e2e_words", key.words());
}

E2EKey load_e2e_key(const std::filesystem::path &dir) {
	return E2EKey::from_words(read_file(dir / "e2e_words"));
}

E2EKey load_or_create_e2e_key(const std::filesystem::path &dir) {
	if (std::filesystem::exists(dir / "e2e_words")) {
		return load_e2e_key(dir);
	}
	auto key = E2EKey::generate();
	save_e2e_key(dir, key);
	return key;
}

} // namespace lan
