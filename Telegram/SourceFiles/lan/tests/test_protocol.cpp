#include "check.h"
#include "protocol.h"

#include <string>
#include <vector>

LAN_TEST(hex_roundtrip) {
	const uint8_t bytes[4] = {0x00, 0xab, 0xcd, 0xff};
	const auto hex = lan::to_hex(bytes, 4);
	LAN_CHECK_EQ(hex, std::string("00abcdff"));
	const auto back = lan::from_hex(hex);
	LAN_REQUIRE(back.has_value());
	LAN_CHECK(std::vector<uint8_t>(bytes, bytes + 4) == *back);
	LAN_CHECK(!lan::from_hex("00ab").has_value() == false);
	LAN_CHECK(!lan::from_hex("00abc").has_value());
	LAN_CHECK(!lan::from_hex("00zz").has_value());
	LAN_CHECK(!lan::from_hex("00AB").has_value()); // lowercase only
}

LAN_TEST(random_hex_id_is_unique_and_valid) {
	const auto a = lan::random_hex_id();
	const auto b = lan::random_hex_id();
	LAN_CHECK_EQ(a.size(), size_t(32));
	LAN_CHECK_EQ(b.size(), size_t(32));
	LAN_CHECK(a != b);
	LAN_CHECK(lan::from_hex(a).has_value());
}

static void expect_roundtrip(const lan::Message &message) {
	const auto encoded = lan::encode_frame(message);
	LAN_REQUIRE(encoded.size() >= 5);
	const auto decoded = lan::try_decode_frame(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->consumed, encoded.size());
	const auto again = lan::encode_frame(decoded->message);
	LAN_CHECK(encoded == again);
}

LAN_TEST(frame_roundtrip_hello_text_ack_bye) {
	expect_roundtrip(lan::Hello{"0123456789abcdef0123456789abcdef", "Alice"});
	expect_roundtrip(lan::Text{lan::random_hex_id(), 1'700'000'000'000ULL, "你好，LAN"});
	expect_roundtrip(lan::Ack{"0123456789abcdef0123456789abcdef"});
	expect_roundtrip(lan::Bye{});
}

LAN_TEST(try_decode_incomplete_returns_empty) {
	const auto encoded = lan::encode_frame(lan::Bye{});
	LAN_CHECK(!lan::try_decode_frame(encoded.data(), 0).has_value());
	LAN_CHECK(!lan::try_decode_frame(encoded.data(), 3).has_value());
	LAN_CHECK(!lan::try_decode_frame(encoded.data(), encoded.size() - 1).has_value());
}

LAN_TEST(try_decode_rejects_oversized_and_truncated_payload) {
	// length prefix claims more than kMaxFramePayload
	std::vector<uint8_t> huge(4, 0);
	huge[0] = 0x00;
	huge[1] = 0x01; // 65536
	huge[2] = 0x00;
	huge[3] = 0x00;
	LAN_CHECK(!lan::try_decode_frame(huge.data(), huge.size()).has_value());

	// valid length but unknown type
	std::vector<uint8_t> bad{0, 0, 0, 1, 99};
	LAN_CHECK(!lan::try_decode_frame(bad.data(), bad.size()).has_value());
}

LAN_TEST(announce_roundtrip) {
	lan::Announce announce;
	announce.peer_id = "0123456789abcdef0123456789abcdef";
	announce.display_name = "车间主机";
	announce.tcp_port = 48711;
	announce.version = lan::kProtocolVersion;
	const auto encoded = lan::encode_announce(announce);
	const auto decoded = lan::decode_announce(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->peer_id, announce.peer_id);
	LAN_CHECK_EQ(decoded->display_name, announce.display_name);
	LAN_CHECK_EQ(decoded->tcp_port, announce.tcp_port);
	LAN_CHECK_EQ(static_cast<int>(decoded->version), static_cast<int>(announce.version));
	LAN_CHECK(!lan::decode_announce(encoded.data(), 3).has_value());
	LAN_CHECK(!lan::decode_announce(nullptr, 0).has_value());
}
