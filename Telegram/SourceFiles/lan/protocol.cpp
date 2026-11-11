#include "protocol.h"

#include "identity.h"
#include "e2e.h"

#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <fstream>
#endif

namespace lan {
namespace {

enum class Type : uint8_t {
	Hello = 1,
	Text = 2,
	Ack = 3,
	Bye = 4,
};

void append_u8(std::vector<uint8_t> &out, uint8_t value) {
	out.push_back(value);
}

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

void append_u64be(std::vector<uint8_t> &out, uint64_t value) {
	append_u32be(out, static_cast<uint32_t>(value >> 32));
	append_u32be(out, static_cast<uint32_t>(value));
}

void append_bytes(std::vector<uint8_t> &out, const uint8_t *data, size_t size) {
	out.insert(out.end(), data, data + size);
}

bool read_u8(const uint8_t *&cursor, const uint8_t *end, uint8_t &value) {
	if (cursor >= end) {
		return false;
	}
	value = *cursor++;
	return true;
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

bool read_u64be(const uint8_t *&cursor, const uint8_t *end, uint64_t &value) {
	uint32_t high = 0;
	uint32_t low = 0;
	if (!read_u32be(cursor, end, high) || !read_u32be(cursor, end, low)) {
		return false;
	}
	value = (static_cast<uint64_t>(high) << 32) | low;
	return true;
}

bool read_bytes(const uint8_t *&cursor, const uint8_t *end, size_t size, std::vector<uint8_t> &out) {
	if (size > static_cast<size_t>(end - cursor)) {
		return false;
	}
	out.assign(cursor, cursor + size);
	cursor += size;
	return true;
}

bool read_hex_id(const uint8_t *&cursor, const uint8_t *end, std::string &out) {
	std::vector<uint8_t> raw;
	if (!read_bytes(cursor, end, 16, raw)) {
		return false;
	}
	out = to_hex(raw.data(), raw.size());
	return true;
}

bool append_hex_id(std::vector<uint8_t> &out, std::string_view hex) {
	const auto raw = from_hex(hex);
	if (!raw || raw->size() != 16) {
		return false;
	}
	append_bytes(out, raw->data(), raw->size());
	return true;
}

char hex_digit(uint8_t value) {
	return "0123456789abcdef"[value & 0x0f];
}

int hex_value(char ch) {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if (ch >= 'a' && ch <= 'f') {
		return ch - 'a' + 10;
	}
	return -1;
}

std::vector<uint8_t> payload_for(const Message &message) {
	std::vector<uint8_t> payload;
	if (const auto *hello = std::get_if<Hello>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::Hello));
		if (!append_hex_id(payload, hello->peer_id)) {
			throw std::invalid_argument("invalid hello peer_id");
		}
		if (!is_valid_display_name(hello->display_name)
			|| hello->display_name.size() > 255) {
			throw std::invalid_argument("invalid hello display_name");
		}
		append_u8(payload, static_cast<uint8_t>(hello->display_name.size()));
		append_bytes(
			payload,
			reinterpret_cast<const uint8_t *>(hello->display_name.data()),
			hello->display_name.size());
		if (!is_valid_public_key(hello->public_key)) {
			throw std::invalid_argument("invalid hello public_key");
		}
		append_bytes(payload, hello->public_key.data(), hello->public_key.size());
		return payload;
	}
	if (const auto *text = std::get_if<Text>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::Text));
		if (!append_hex_id(payload, text->msg_id)) {
			throw std::invalid_argument("invalid text msg_id");
		}
		append_u64be(payload, text->timestamp_ms);
		if (text->body.size() > kMaxFramePayload) {
			throw std::invalid_argument("text body too large");
		}
		append_u32be(payload, static_cast<uint32_t>(text->body.size()));
		append_bytes(
			payload,
			reinterpret_cast<const uint8_t *>(text->body.data()),
			text->body.size());
		return payload;
	}
	if (const auto *ack = std::get_if<Ack>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::Ack));
		if (!append_hex_id(payload, ack->msg_id)) {
			throw std::invalid_argument("invalid ack msg_id");
		}
		return payload;
	}
	append_u8(payload, static_cast<uint8_t>(Type::Bye));
	return payload;
}

} // namespace

std::string to_hex(const uint8_t *bytes, size_t size) {
	std::string hex;
	hex.resize(size * 2);
	for (size_t i = 0; i < size; ++i) {
		hex[i * 2] = hex_digit(bytes[i] >> 4);
		hex[i * 2 + 1] = hex_digit(bytes[i]);
	}
	return hex;
}

std::optional<std::vector<uint8_t>> from_hex(std::string_view hex) {
	if (hex.size() % 2 != 0) {
		return std::nullopt;
	}
	std::vector<uint8_t> out;
	out.reserve(hex.size() / 2);
	for (size_t i = 0; i < hex.size(); i += 2) {
		const int high = hex_value(hex[i]);
		const int low = hex_value(hex[i + 1]);
		if (high < 0 || low < 0) {
			return std::nullopt;
		}
		out.push_back(static_cast<uint8_t>((high << 4) | low));
	}
	return out;
}

std::string random_hex_id() {
	uint8_t bytes[16] = {};
#if defined(_WIN32)
	const NTSTATUS status = BCryptGenRandom(
		nullptr,
		bytes,
		sizeof(bytes),
		BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (status != 0) {
		throw std::runtime_error("failed to generate random id");
	}
#else
	std::ifstream urandom("/dev/urandom", std::ios::binary);
	if (!urandom.read(reinterpret_cast<char *>(bytes), sizeof(bytes))) {
		throw std::runtime_error("failed to generate random id");
	}
#endif
	return to_hex(bytes, sizeof(bytes));
}

std::vector<uint8_t> encode_frame(const Message &message) {
	const auto payload = payload_for(message);
	if (payload.size() > kMaxFramePayload) {
		throw std::invalid_argument("frame payload too large");
	}
	std::vector<uint8_t> frame;
	append_u32be(frame, static_cast<uint32_t>(payload.size()));
	append_bytes(frame, payload.data(), payload.size());
	return frame;
}

std::optional<DecodeResult> try_decode_frame(const uint8_t *data, size_t size) {
	if (data == nullptr || size < 4) {
		return std::nullopt;
	}
	const uint8_t *cursor = data;
	const uint8_t *end = data + size;
	uint32_t payload_size = 0;
	if (!read_u32be(cursor, end, payload_size)) {
		return std::nullopt;
	}
	if (payload_size == 0 || payload_size > kMaxFramePayload) {
		return std::nullopt;
	}
	if (static_cast<size_t>(end - cursor) < payload_size) {
		return std::nullopt;
	}
	const uint8_t *payload_end = cursor + payload_size;
	uint8_t type = 0;
	if (!read_u8(cursor, payload_end, type)) {
		return std::nullopt;
	}
	Message message;
	if (type == static_cast<uint8_t>(Type::Hello)) {
		Hello hello;
		uint8_t name_len = 0;
		if (!read_hex_id(cursor, payload_end, hello.peer_id)
			|| !read_u8(cursor, payload_end, name_len)) {
			return std::nullopt;
		}
		std::vector<uint8_t> name;
		if (!read_bytes(cursor, payload_end, name_len, name)) {
			return std::nullopt;
		}
		hello.display_name.assign(
			reinterpret_cast<const char *>(name.data()),
			name.size());
		if (!read_bytes(cursor, payload_end, kE2EPublicKeySize, hello.public_key)
			|| !is_valid_peer_id(hello.peer_id)
			|| !is_valid_display_name(hello.display_name)
			|| !is_valid_public_key(hello.public_key)) {
			return std::nullopt;
		}
		message = std::move(hello);
	} else if (type == static_cast<uint8_t>(Type::Text)) {
		Text text;
		uint32_t body_len = 0;
		if (!read_hex_id(cursor, payload_end, text.msg_id)
			|| !read_u64be(cursor, payload_end, text.timestamp_ms)
			|| !read_u32be(cursor, payload_end, body_len)
			|| body_len > kMaxFramePayload) {
			return std::nullopt;
		}
		std::vector<uint8_t> body;
		if (!read_bytes(cursor, payload_end, body_len, body)) {
			return std::nullopt;
		}
		if (!is_valid_peer_id(text.msg_id)) {
			return std::nullopt;
		}
		text.body.assign(reinterpret_cast<const char *>(body.data()), body.size());
		message = std::move(text);
	} else if (type == static_cast<uint8_t>(Type::Ack)) {
		Ack ack;
		if (!read_hex_id(cursor, payload_end, ack.msg_id)
			|| !is_valid_peer_id(ack.msg_id)) {
			return std::nullopt;
		}
		message = std::move(ack);
	} else if (type == static_cast<uint8_t>(Type::Bye)) {
		message = Bye{};
	} else {
		return std::nullopt;
	}
	if (cursor != payload_end) {
		return std::nullopt;
	}
	return DecodeResult{std::move(message), static_cast<size_t>(payload_end - data)};
}

std::vector<uint8_t> encode_announce(const Announce &announce) {
	if (!is_valid_peer_id(announce.peer_id)
		|| !is_valid_display_name(announce.display_name)
		|| announce.display_name.size() > 255
		|| announce.tcp_port == 0) {
		throw std::invalid_argument("invalid announce");
	}
	const auto raw = from_hex(announce.peer_id);
	std::vector<uint8_t> packet;
	append_bytes(
		packet,
		reinterpret_cast<const uint8_t *>(kAnnounceMagic),
		4);
	append_u8(packet, announce.version);
	append_u16be(packet, announce.tcp_port);
	append_bytes(packet, raw->data(), raw->size());
	if (!is_valid_public_key(announce.public_key)) {
		throw std::invalid_argument("invalid announce");
	}
	append_u8(packet, static_cast<uint8_t>(announce.display_name.size()));
	append_bytes(
		packet,
		reinterpret_cast<const uint8_t *>(announce.display_name.data()),
		announce.display_name.size());
	append_bytes(packet, announce.public_key.data(), announce.public_key.size());
	return packet;
}

std::optional<Announce> decode_announce(const uint8_t *data, size_t size) {
	if (data == nullptr || size < 4 + 1 + 2 + 16 + 1 + kE2EPublicKeySize) {
		return std::nullopt;
	}
	if (data[0] != kAnnounceMagic[0]
		|| data[1] != kAnnounceMagic[1]
		|| data[2] != kAnnounceMagic[2]
		|| data[3] != kAnnounceMagic[3]) {
		return std::nullopt;
	}
	const uint8_t *cursor = data + 4;
	const uint8_t *end = data + size;
	Announce announce;
	uint8_t name_len = 0;
	if (!read_u8(cursor, end, announce.version)
		|| announce.version != kProtocolVersion
		|| !read_u16be(cursor, end, announce.tcp_port)
		|| announce.tcp_port == 0
		|| !read_hex_id(cursor, end, announce.peer_id)
		|| !read_u8(cursor, end, name_len)
		|| !is_valid_peer_id(announce.peer_id)) {
		return std::nullopt;
	}
	std::vector<uint8_t> name;
	if (!read_bytes(cursor, end, name_len, name)) {
		return std::nullopt;
	}
	announce.display_name.assign(
		reinterpret_cast<const char *>(name.data()),
		name.size());
	if (!is_valid_display_name(announce.display_name)
		|| !read_bytes(cursor, end, kE2EPublicKeySize, announce.public_key)
		|| !is_valid_public_key(announce.public_key)
		|| cursor != end) {
		return std::nullopt;
	}
	return announce;
}

} // namespace lan
