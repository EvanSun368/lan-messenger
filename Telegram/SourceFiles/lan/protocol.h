#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lan {

constexpr uint8_t kProtocolVersion = 1;
constexpr uint32_t kMaxFramePayload = 64 * 1024;
inline constexpr char kAnnounceMagic[4] = {'L', 'N', 'K', 'L'};

struct Hello {
	std::string peer_id; // 32 hex
	std::string display_name;
};

struct Text {
	std::string msg_id; // 32 hex
	uint64_t timestamp_ms = 0;
	std::string body;
};

struct Ack {
	std::string msg_id; // 32 hex
};

struct Bye {};

using Message = std::variant<Hello, Text, Ack, Bye>;

struct DecodeResult {
	Message message;
	size_t consumed = 0;
};

// TCP frames: 4-byte big-endian payload length, then type + fields.
std::vector<uint8_t> encode_frame(const Message &message);
std::optional<DecodeResult> try_decode_frame(
	const uint8_t *data,
	size_t size);

struct Announce {
	std::string peer_id; // 32 hex
	std::string display_name;
	uint16_t tcp_port = 0;
	uint8_t version = kProtocolVersion;
};

// UDP discovery datagram. Not length-prefixed; one datagram is one announce.
std::vector<uint8_t> encode_announce(const Announce &announce);
std::optional<Announce> decode_announce(const uint8_t *data, size_t size);

std::string random_hex_id();
std::string to_hex(const uint8_t *bytes, size_t size);
std::optional<std::vector<uint8_t>> from_hex(std::string_view hex);

} // namespace lan
