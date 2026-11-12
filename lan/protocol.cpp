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
	FileOffer = 5,
	FileChunk = 6,
	FileAck = 7,
	FileComplete = 8,
	FileAbort = 9,
	FolderOffer = 10,
	FolderAck = 11,
	FolderComplete = 12,
	FolderAbort = 13,
	GroupInvite = 14,
	GroupUpdate = 15,
	GroupText = 16,
	Recall = 17,
	FileDigest = 18,
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

bool valid_profile_field(std::string_view text, size_t max_size) {
	if (text.size() > max_size) {
		return false;
	}
	for (unsigned char ch : text) {
		if (ch < 0x20 && ch != '\t') {
			return false;
		}
	}
	return true;
}

void append_profile_fields(
		std::vector<uint8_t> &out,
		std::string_view first,
		std::string_view last,
		std::string_view bio) {
	if (first.empty() && last.empty() && bio.empty()) {
		return;
	}
	append_u8(out, static_cast<uint8_t>(first.size()));
	append_bytes(out, reinterpret_cast<const uint8_t *>(first.data()), first.size());
	append_u8(out, static_cast<uint8_t>(last.size()));
	append_bytes(out, reinterpret_cast<const uint8_t *>(last.data()), last.size());
	append_u16be(out, static_cast<uint16_t>(bio.size()));
	append_bytes(out, reinterpret_cast<const uint8_t *>(bio.data()), bio.size());
}

bool read_profile_fields(
		const uint8_t *&cursor,
		const uint8_t *end,
		std::string &first,
		std::string &last,
		std::string &bio) {
	if (cursor == end) {
		return true;
	}
	uint8_t first_len = 0;
	uint8_t last_len = 0;
	uint16_t bio_len = 0;
	std::vector<uint8_t> first_raw;
	std::vector<uint8_t> last_raw;
	std::vector<uint8_t> bio_raw;
	if (!read_u8(cursor, end, first_len)
		|| !read_bytes(cursor, end, first_len, first_raw)
		|| !read_u8(cursor, end, last_len)
		|| !read_bytes(cursor, end, last_len, last_raw)
		|| !read_u16be(cursor, end, bio_len)
		|| !read_bytes(cursor, end, bio_len, bio_raw)) {
		return false;
	}
	first.assign(reinterpret_cast<const char *>(first_raw.data()), first_raw.size());
	last.assign(reinterpret_cast<const char *>(last_raw.data()), last_raw.size());
	bio.assign(reinterpret_cast<const char *>(bio_raw.data()), bio_raw.size());
	return valid_profile_field(first, 64)
		&& valid_profile_field(last, 64)
		&& valid_profile_field(bio, 140);
}

uint32_t max_payload_for(const Message &message) {
	if (std::holds_alternative<FileChunk>(message)) {
		return kMaxFileFramePayload;
	}
	if (std::holds_alternative<Text>(message)
		|| std::holds_alternative<GroupText>(message)) {
		return kMaxTextFramePayload;
	}
	return kMaxFramePayload;
}

uint32_t max_payload_for_type(uint8_t type) {
	if (type == static_cast<uint8_t>(Type::FileChunk)) {
		return kMaxFileFramePayload;
	}
	if (type == static_cast<uint8_t>(Type::Text)
		|| type == static_cast<uint8_t>(Type::GroupText)) {
		return kMaxTextFramePayload;
	}
	return kMaxFramePayload;
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
		if (!valid_profile_field(hello->first_name, 64)
			|| !valid_profile_field(hello->last_name, 64)
			|| !valid_profile_field(hello->bio, 140)) {
			throw std::invalid_argument("invalid hello profile");
		}
		append_profile_fields(payload, hello->first_name, hello->last_name, hello->bio);
		return payload;
	}
	if (const auto *text = std::get_if<Text>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::Text));
		if (!append_hex_id(payload, text->msg_id)) {
			throw std::invalid_argument("invalid text msg_id");
		}
		append_u64be(payload, text->timestamp_ms);
		if (text->body.size() > kMaxTextFramePayload) {
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
	if (const auto *offer = std::get_if<FileOffer>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FileOffer));
		if (!append_hex_id(payload, offer->transfer_id)) {
			throw std::invalid_argument("invalid file offer id");
		}
		append_u64be(payload, offer->size);
		append_u32be(payload, offer->chunk_count);
		if (offer->sealed.size() > kMaxFramePayload) {
			throw std::invalid_argument("file offer too large");
		}
		append_u32be(payload, static_cast<uint32_t>(offer->sealed.size()));
		append_bytes(payload, offer->sealed.data(), offer->sealed.size());
		return payload;
	}
	if (const auto *chunk = std::get_if<FileChunk>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FileChunk));
		if (!append_hex_id(payload, chunk->transfer_id)) {
			throw std::invalid_argument("invalid file chunk id");
		}
		append_u32be(payload, chunk->index);
		if (chunk->sealed.size() > kMaxFileFramePayload) {
			throw std::invalid_argument("file chunk too large");
		}
		append_u32be(payload, static_cast<uint32_t>(chunk->sealed.size()));
		append_bytes(payload, chunk->sealed.data(), chunk->sealed.size());
		return payload;
	}
	if (const auto *ack = std::get_if<FileAck>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FileAck));
		if (!append_hex_id(payload, ack->transfer_id)) {
			throw std::invalid_argument("invalid file ack id");
		}
		append_u32be(payload, ack->acked_count);
		return payload;
	}
	if (const auto *done = std::get_if<FileComplete>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FileComplete));
		if (!append_hex_id(payload, done->transfer_id)) {
			throw std::invalid_argument("invalid file complete id");
		}
		return payload;
	}
	if (const auto *abort = std::get_if<FileAbort>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FileAbort));
		if (!append_hex_id(payload, abort->transfer_id)) {
			throw std::invalid_argument("invalid file abort id");
		}
		append_u8(payload, abort->reason);
		return payload;
	}
	if (const auto *digest = std::get_if<FileDigest>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FileDigest));
		if (!append_hex_id(payload, digest->transfer_id)) {
			throw std::invalid_argument("invalid file digest id");
		}
		if (digest->sha256.size() != 32) {
			throw std::invalid_argument("invalid file digest");
		}
		append_bytes(payload, digest->sha256.data(), digest->sha256.size());
		return payload;
	}
	if (const auto *folder = std::get_if<FolderOffer>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FolderOffer));
		if (!append_hex_id(payload, folder->folder_id)) {
			throw std::invalid_argument("invalid folder offer id");
		}
		append_u32be(payload, folder->file_count);
		if (folder->sealed.size() > kMaxFramePayload) {
			throw std::invalid_argument("folder offer too large");
		}
		append_u32be(payload, static_cast<uint32_t>(folder->sealed.size()));
		append_bytes(payload, folder->sealed.data(), folder->sealed.size());
		return payload;
	}
	if (const auto *ack = std::get_if<FolderAck>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FolderAck));
		if (!append_hex_id(payload, ack->folder_id)) {
			throw std::invalid_argument("invalid folder ack id");
		}
		append_u32be(payload, ack->acked_files);
		return payload;
	}
	if (const auto *done = std::get_if<FolderComplete>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FolderComplete));
		if (!append_hex_id(payload, done->folder_id)) {
			throw std::invalid_argument("invalid folder complete id");
		}
		return payload;
	}
	if (const auto *abort = std::get_if<FolderAbort>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::FolderAbort));
		if (!append_hex_id(payload, abort->folder_id)) {
			throw std::invalid_argument("invalid folder abort id");
		}
		append_u8(payload, abort->reason);
		return payload;
	}
	if (const auto *invite = std::get_if<GroupInvite>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::GroupInvite));
		if (!append_hex_id(payload, invite->group_id)) {
			throw std::invalid_argument("invalid group invite id");
		}
		if (invite->sealed.size() > kMaxFramePayload) {
			throw std::invalid_argument("group invite too large");
		}
		append_u32be(payload, static_cast<uint32_t>(invite->sealed.size()));
		append_bytes(payload, invite->sealed.data(), invite->sealed.size());
		return payload;
	}
	if (const auto *update = std::get_if<GroupUpdate>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::GroupUpdate));
		if (!append_hex_id(payload, update->group_id)) {
			throw std::invalid_argument("invalid group update id");
		}
		if (update->sealed.size() > kMaxFramePayload) {
			throw std::invalid_argument("group update too large");
		}
		append_u32be(payload, static_cast<uint32_t>(update->sealed.size()));
		append_bytes(payload, update->sealed.data(), update->sealed.size());
		return payload;
	}
	if (const auto *text = std::get_if<GroupText>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::GroupText));
		if (!append_hex_id(payload, text->group_id)) {
			throw std::invalid_argument("invalid group text group id");
		}
		if (!append_hex_id(payload, text->msg_id)) {
			throw std::invalid_argument("invalid group text msg id");
		}
		append_u64be(payload, text->timestamp_ms);
		if (text->sealed.size() > kMaxTextFramePayload) {
			throw std::invalid_argument("group text too large");
		}
		append_u32be(payload, static_cast<uint32_t>(text->sealed.size()));
		append_bytes(payload, text->sealed.data(), text->sealed.size());
		return payload;
	}
	if (const auto *recall = std::get_if<Recall>(&message)) {
		append_u8(payload, static_cast<uint8_t>(Type::Recall));
		if (!append_hex_id(payload, recall->msg_id)) {
			throw std::invalid_argument("invalid recall msg id");
		}
		if (recall->group_id.empty()) {
			append_u8(payload, 0);
		} else {
			append_u8(payload, 1);
			if (!append_hex_id(payload, recall->group_id)) {
				throw std::invalid_argument("invalid recall group id");
			}
		}
		return payload;
	}
	if (std::holds_alternative<Bye>(message)) {
		append_u8(payload, static_cast<uint8_t>(Type::Bye));
		return payload;
	}
	throw std::invalid_argument("unknown message type");
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
	if (payload.size() > max_payload_for(message)) {
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
	if (payload_size == 0 || payload_size > kMaxAnyFramePayload) {
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
	if (payload_size > max_payload_for_type(type)) {
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
			|| !is_valid_public_key(hello.public_key)
			|| !read_profile_fields(
				cursor,
				payload_end,
				hello.first_name,
				hello.last_name,
				hello.bio)) {
			return std::nullopt;
		}
		message = std::move(hello);
	} else if (type == static_cast<uint8_t>(Type::Text)) {
		Text text;
		uint32_t body_len = 0;
		if (!read_hex_id(cursor, payload_end, text.msg_id)
			|| !read_u64be(cursor, payload_end, text.timestamp_ms)
			|| !read_u32be(cursor, payload_end, body_len)
			|| body_len > kMaxTextFramePayload) {
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
	} else if (type == static_cast<uint8_t>(Type::FileOffer)) {
		FileOffer offer;
		uint32_t sealed_len = 0;
		if (!read_hex_id(cursor, payload_end, offer.transfer_id)
			|| !is_valid_peer_id(offer.transfer_id)
			|| !read_u64be(cursor, payload_end, offer.size)
			|| !read_u32be(cursor, payload_end, offer.chunk_count)
			|| !read_u32be(cursor, payload_end, sealed_len)
			|| sealed_len > kMaxFramePayload
			|| !read_bytes(cursor, payload_end, sealed_len, offer.sealed)) {
			return std::nullopt;
		}
		message = std::move(offer);
	} else if (type == static_cast<uint8_t>(Type::FileChunk)) {
		FileChunk chunk;
		uint32_t sealed_len = 0;
		if (!read_hex_id(cursor, payload_end, chunk.transfer_id)
			|| !is_valid_peer_id(chunk.transfer_id)
			|| !read_u32be(cursor, payload_end, chunk.index)
			|| !read_u32be(cursor, payload_end, sealed_len)
			|| sealed_len > kMaxFileFramePayload
			|| !read_bytes(cursor, payload_end, sealed_len, chunk.sealed)) {
			return std::nullopt;
		}
		message = std::move(chunk);
	} else if (type == static_cast<uint8_t>(Type::FileAck)) {
		FileAck ack;
		if (!read_hex_id(cursor, payload_end, ack.transfer_id)
			|| !is_valid_peer_id(ack.transfer_id)
			|| !read_u32be(cursor, payload_end, ack.acked_count)) {
			return std::nullopt;
		}
		message = std::move(ack);
	} else if (type == static_cast<uint8_t>(Type::FileComplete)) {
		FileComplete done;
		if (!read_hex_id(cursor, payload_end, done.transfer_id)
			|| !is_valid_peer_id(done.transfer_id)) {
			return std::nullopt;
		}
		message = std::move(done);
	} else if (type == static_cast<uint8_t>(Type::FileDigest)) {
		FileDigest digest;
		if (!read_hex_id(cursor, payload_end, digest.transfer_id)
			|| !is_valid_peer_id(digest.transfer_id)
			|| !read_bytes(cursor, payload_end, 32, digest.sha256)) {
			return std::nullopt;
		}
		message = std::move(digest);
	} else if (type == static_cast<uint8_t>(Type::FileAbort)) {
		FileAbort abort;
		if (!read_hex_id(cursor, payload_end, abort.transfer_id)
			|| !is_valid_peer_id(abort.transfer_id)
			|| !read_u8(cursor, payload_end, abort.reason)) {
			return std::nullopt;
		}
		message = std::move(abort);
	} else if (type == static_cast<uint8_t>(Type::FolderOffer)) {
		FolderOffer offer;
		uint32_t sealed_len = 0;
		if (!read_hex_id(cursor, payload_end, offer.folder_id)
			|| !is_valid_peer_id(offer.folder_id)
			|| !read_u32be(cursor, payload_end, offer.file_count)
			|| !read_u32be(cursor, payload_end, sealed_len)
			|| sealed_len > kMaxFramePayload
			|| !read_bytes(cursor, payload_end, sealed_len, offer.sealed)) {
			return std::nullopt;
		}
		message = std::move(offer);
	} else if (type == static_cast<uint8_t>(Type::FolderAck)) {
		FolderAck ack;
		if (!read_hex_id(cursor, payload_end, ack.folder_id)
			|| !is_valid_peer_id(ack.folder_id)
			|| !read_u32be(cursor, payload_end, ack.acked_files)) {
			return std::nullopt;
		}
		message = std::move(ack);
	} else if (type == static_cast<uint8_t>(Type::FolderComplete)) {
		FolderComplete done;
		if (!read_hex_id(cursor, payload_end, done.folder_id)
			|| !is_valid_peer_id(done.folder_id)) {
			return std::nullopt;
		}
		message = std::move(done);
	} else if (type == static_cast<uint8_t>(Type::FolderAbort)) {
		FolderAbort abort;
		if (!read_hex_id(cursor, payload_end, abort.folder_id)
			|| !is_valid_peer_id(abort.folder_id)
			|| !read_u8(cursor, payload_end, abort.reason)) {
			return std::nullopt;
		}
		message = std::move(abort);
	} else if (type == static_cast<uint8_t>(Type::GroupInvite)) {
		GroupInvite invite;
		uint32_t sealed_len = 0;
		if (!read_hex_id(cursor, payload_end, invite.group_id)
			|| !is_valid_peer_id(invite.group_id)
			|| !read_u32be(cursor, payload_end, sealed_len)
			|| sealed_len > kMaxFramePayload
			|| !read_bytes(cursor, payload_end, sealed_len, invite.sealed)) {
			return std::nullopt;
		}
		message = std::move(invite);
	} else if (type == static_cast<uint8_t>(Type::GroupUpdate)) {
		GroupUpdate update;
		uint32_t sealed_len = 0;
		if (!read_hex_id(cursor, payload_end, update.group_id)
			|| !is_valid_peer_id(update.group_id)
			|| !read_u32be(cursor, payload_end, sealed_len)
			|| sealed_len > kMaxFramePayload
			|| !read_bytes(cursor, payload_end, sealed_len, update.sealed)) {
			return std::nullopt;
		}
		message = std::move(update);
	} else if (type == static_cast<uint8_t>(Type::GroupText)) {
		GroupText text;
		uint32_t sealed_len = 0;
		if (!read_hex_id(cursor, payload_end, text.group_id)
			|| !is_valid_peer_id(text.group_id)
			|| !read_hex_id(cursor, payload_end, text.msg_id)
			|| !is_valid_peer_id(text.msg_id)
			|| !read_u64be(cursor, payload_end, text.timestamp_ms)
			|| !read_u32be(cursor, payload_end, sealed_len)
			|| sealed_len > kMaxTextFramePayload
			|| !read_bytes(cursor, payload_end, sealed_len, text.sealed)) {
			return std::nullopt;
		}
		message = std::move(text);
	} else if (type == static_cast<uint8_t>(Type::Recall)) {
		Recall recall;
		uint8_t has_group = 0;
		if (!read_hex_id(cursor, payload_end, recall.msg_id)
			|| !is_valid_peer_id(recall.msg_id)
			|| !read_u8(cursor, payload_end, has_group)) {
			return std::nullopt;
		}
		if (has_group == 1) {
			if (!read_hex_id(cursor, payload_end, recall.group_id)
				|| !is_valid_peer_id(recall.group_id)) {
				return std::nullopt;
			}
		} else if (has_group != 0) {
			return std::nullopt;
		}
		message = std::move(recall);
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
	if (!valid_profile_field(announce.first_name, 64)
		|| !valid_profile_field(announce.last_name, 64)
		|| !valid_profile_field(announce.bio, 140)) {
		throw std::invalid_argument("invalid announce profile");
	}
	append_profile_fields(
		packet,
		announce.first_name,
		announce.last_name,
		announce.bio);
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
		|| !read_profile_fields(
			cursor,
			end,
			announce.first_name,
			announce.last_name,
			announce.bio)
		|| cursor != end) {
		return std::nullopt;
	}
	return announce;
}

} // namespace lan
