#include "check.h"
#include "protocol.h"
#include "transfer.h"
#include "e2e.h"

#include <string>
#include <vector>

namespace {
std::vector<uint8_t> dummy_key(uint8_t tag) {
	return std::vector<uint8_t>(lan::kE2EPublicKeySize, tag);
}
} // namespace

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
	expect_roundtrip(lan::Hello{
		"0123456789abcdef0123456789abcdef",
		"Alice",
		dummy_key(0x11)});
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
	// length prefix claims more than kMaxAnyFramePayload
	std::vector<uint8_t> huge(4, 0);
	const uint32_t too_big = lan::kMaxAnyFramePayload + 1;
	huge[0] = static_cast<uint8_t>(too_big >> 24);
	huge[1] = static_cast<uint8_t>(too_big >> 16);
	huge[2] = static_cast<uint8_t>(too_big >> 8);
	huge[3] = static_cast<uint8_t>(too_big);
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
	announce.public_key = dummy_key(0x22);
	const auto encoded = lan::encode_announce(announce);
	const auto decoded = lan::decode_announce(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->peer_id, announce.peer_id);
	LAN_CHECK_EQ(decoded->display_name, announce.display_name);
	LAN_CHECK_EQ(decoded->tcp_port, announce.tcp_port);
	LAN_CHECK_EQ(static_cast<int>(decoded->version), static_cast<int>(announce.version));
	LAN_CHECK(decoded->public_key == announce.public_key);
	LAN_CHECK(!lan::decode_announce(encoded.data(), 3).has_value());
	LAN_CHECK(!lan::decode_announce(nullptr, 0).has_value());
}

LAN_TEST(announce_rejects_version_1_plaintext_packets) {
	// v1 layout: magic + version + port + peer_id + name, no public key.
	std::vector<uint8_t> v1 = {
		'L', 'N', 'K', 'L',
		1, // version 1
		0xbe, 0x27, // port 48711
	};
	v1.insert(v1.end(), 16, 0xab);
	v1.push_back(5);
	v1.insert(v1.end(), {'A', 'l', 'i', 'c', 'e'});
	LAN_CHECK(!lan::decode_announce(v1.data(), v1.size()).has_value());
}

LAN_TEST(encrypted_text_frame_does_not_contain_plaintext) {
	const auto alice = lan::E2EKey::generate();
	const auto bob = lan::E2EKey::generate();
	const std::string plain = "unique-plaintext-needle-xyz";
	const auto cipher = alice.encrypt_for(bob.public_key(), plain);
	lan::Text text;
	text.msg_id = lan::random_hex_id();
	text.timestamp_ms = 42;
	text.body.assign(cipher.begin(), cipher.end());
	const auto frame = lan::encode_frame(text);
	const std::string wire(frame.begin(), frame.end());
	LAN_CHECK(wire.find(plain) == std::string::npos);
	const auto decoded = lan::try_decode_frame(frame.data(), frame.size());
	LAN_REQUIRE(decoded.has_value());
	const auto *got = std::get_if<lan::Text>(&decoded->message);
	LAN_REQUIRE(got != nullptr);
	const auto opened = bob.decrypt_from(
		alice.public_key(),
		std::string_view(got->body));
	LAN_REQUIRE(opened.has_value());
	LAN_CHECK_EQ(*opened, plain);
}

LAN_TEST(frame_roundtrip_file_messages) {
	const auto id = lan::random_hex_id();
	expect_roundtrip(lan::FileOffer{id, 1234, 2, std::vector<uint8_t>{1, 2, 3}});
	expect_roundtrip(lan::FileChunk{id, 1, std::vector<uint8_t>(16, 7)});
	expect_roundtrip(lan::FileAck{id, 1});
	expect_roundtrip(lan::FileComplete{id});
	expect_roundtrip(lan::FileAbort{id, 9});
}

LAN_TEST(frame_roundtrip_folder_and_group_messages) {
	const auto id = lan::random_hex_id();
	const auto msg = lan::random_hex_id();
	expect_roundtrip(lan::FolderOffer{id, 4, std::vector<uint8_t>{9, 8, 7}});
	expect_roundtrip(lan::FolderAck{id, 2});
	expect_roundtrip(lan::FolderComplete{id});
	expect_roundtrip(lan::FolderAbort{id, 3});
	expect_roundtrip(lan::GroupInvite{id, std::vector<uint8_t>{1, 2}});
	expect_roundtrip(lan::GroupUpdate{id, std::vector<uint8_t>{3}});
	expect_roundtrip(lan::GroupText{id, msg, 99, std::vector<uint8_t>{4, 5, 6}});
}

LAN_TEST(frame_roundtrip_recall_one_to_one_and_group) {
	const auto msg = lan::random_hex_id();
	const auto group = lan::random_hex_id();
	expect_roundtrip(lan::Recall{msg, {}});
	expect_roundtrip(lan::Recall{msg, group});
}

LAN_TEST(recall_window_is_forty_eight_hours) {
	const uint64_t sent = 1'000'000;
	LAN_CHECK(lan::within_recall_window(sent, sent));
	LAN_CHECK(lan::within_recall_window(sent, sent + lan::kRecallWindowMs));
	LAN_CHECK(!lan::within_recall_window(sent, sent + lan::kRecallWindowMs + 1));
	LAN_CHECK(!lan::within_recall_window(sent, sent - 1));
	LAN_CHECK_EQ(lan::kRecallWindowMs, 48ull * 60ull * 60ull * 1000ull);
}

LAN_TEST(hello_and_announce_roundtrip_profile_fields) {
	lan::Hello hello;
	hello.peer_id = "0123456789abcdef0123456789abcdef";
	hello.display_name = "Ada Lovelace";
	hello.public_key = dummy_key(0x33);
	hello.first_name = "Ada";
	hello.last_name = "Lovelace";
	hello.bio = "Mathematician on the LAN";
	expect_roundtrip(hello);

	lan::Announce announce;
	announce.peer_id = hello.peer_id;
	announce.display_name = hello.display_name;
	announce.tcp_port = 48711;
	announce.public_key = hello.public_key;
	announce.first_name = hello.first_name;
	announce.last_name = hello.last_name;
	announce.bio = hello.bio;
	const auto encoded = lan::encode_announce(announce);
	const auto decoded = lan::decode_announce(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->first_name, announce.first_name);
	LAN_CHECK_EQ(decoded->last_name, announce.last_name);
	LAN_CHECK_EQ(decoded->bio, announce.bio);
}

LAN_TEST(plain_file_chunk_can_exceed_old_frame_limit) {
	lan::FileChunk chunk;
	chunk.transfer_id = "0123456789abcdef0123456789abcdef";
	chunk.index = 0;
	chunk.sealed.assign(lan::kFileChunkPlainFast, uint8_t('P'));
	const auto encoded = lan::encode_frame(chunk);
	LAN_CHECK(encoded.size() > lan::kMaxFramePayload);
	LAN_CHECK(encoded.size() <= 4 + lan::kMaxFileFramePayload);
	const auto decoded = lan::try_decode_frame(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	const auto *out = std::get_if<lan::FileChunk>(&decoded->message);
	LAN_REQUIRE(out != nullptr);
	LAN_CHECK_EQ(out->sealed.size(), size_t(lan::kFileChunkPlainFast));

	// A Hello frame still cannot exceed 64 KiB.
	std::vector<uint8_t> huge(4 + lan::kMaxFramePayload + 2, 0);
	const uint32_t payload = lan::kMaxFramePayload + 1;
	huge[0] = static_cast<uint8_t>(payload >> 24);
	huge[1] = static_cast<uint8_t>(payload >> 16);
	huge[2] = static_cast<uint8_t>(payload >> 8);
	huge[3] = static_cast<uint8_t>(payload);
	huge[4] = 1; // Hello
	LAN_CHECK(!lan::try_decode_frame(huge.data(), huge.size()).has_value());
}

LAN_TEST(text_frame_can_exceed_old_64kib_limit) {
	lan::Text text;
	text.msg_id = "0123456789abcdef0123456789abcdef";
	text.timestamp_ms = 1;
	text.body.assign(lan::kMaxFramePayload + 4096, 'T');
	const auto encoded = lan::encode_frame(text);
	LAN_CHECK(encoded.size() > 4 + lan::kMaxFramePayload);
	LAN_CHECK(encoded.size() <= 4 + lan::kMaxTextFramePayload);
	const auto decoded = lan::try_decode_frame(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	const auto *out = std::get_if<lan::Text>(&decoded->message);
	LAN_REQUIRE(out != nullptr);
	LAN_CHECK_EQ(out->body.size(), text.body.size());
	LAN_CHECK(out->body == text.body);
}

LAN_TEST(file_digest_frame_roundtrip) {
	lan::FileDigest digest;
	digest.transfer_id = "0123456789abcdef0123456789abcdef";
	digest.sha256.assign(32, 0x5a);
	const auto encoded = lan::encode_frame(digest);
	const auto decoded = lan::try_decode_frame(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	const auto *out = std::get_if<lan::FileDigest>(&decoded->message);
	LAN_REQUIRE(out != nullptr);
	LAN_CHECK_EQ(out->transfer_id, digest.transfer_id);
	LAN_CHECK(out->sha256 == digest.sha256);
}
