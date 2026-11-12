#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lan {

constexpr uint8_t kProtocolVersion = 3;
constexpr uint32_t kMaxFramePayload = 64 * 1024;
// FileChunk frames may be larger so the unencrypted fast path can use
// kFileChunkPlainFast payloads. Text/GroupText may also exceed 64 KiB
// so a long message is not dropped. Other control frames stay at 64 KiB.
constexpr uint32_t kMaxTextFramePayload = 1024 * 1024;
constexpr uint32_t kMaxTextPlain = 1024 * 1024;
constexpr uint32_t kMaxFileFramePayload = 4 * 1024 * 1024;
constexpr uint32_t kMaxAnyFramePayload = kMaxFileFramePayload;
inline constexpr char kAnnounceMagic[4] = {'L', 'N', 'K', 'L'};

struct Hello {
	std::string peer_id; // 32 hex
	std::string display_name;
	// Official tde2e public key, 32 raw bytes.
	std::vector<uint8_t> public_key;
	// Optional profile fields announced after the public key. Empty on
	// older peers. first_name / last_name are folded into display_name
	// when present; bio is shown in the profile UI.
	std::string first_name;
	std::string last_name;
	std::string bio;
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

// File transfer frames. Confidential fields (filename, mime, digest, and
// chunk bytes) travel in `sealed` as a tde2e envelope. Cleartext is only
// the transfer id, the total size, the chunk count, and a chunk index.
// `sealed` is a tde2e envelope when the sender encrypts file transfers,
// or an LLP1 plaintext wrapper (and raw chunk bytes) when encryption is
// off. The offer tells the receiver which path to use.
struct FileOffer {
	std::string transfer_id; // 32 hex
	uint64_t size = 0;
	uint32_t chunk_count = 0;
	std::vector<uint8_t> sealed;
};

struct FileChunk {
	std::string transfer_id;
	uint32_t index = 0;
	std::vector<uint8_t> sealed;
};

// acked_count is the number of file chunks the receiver has accepted
// (0 after the offer is accepted, chunk_count when the file is complete).
struct FileAck {
	std::string transfer_id;
	uint32_t acked_count = 0;
};

struct FileComplete {
	std::string transfer_id;
};

struct FileAbort {
	std::string transfer_id;
	uint8_t reason = 0;
};

// Receiver declined a file/folder offer. Older peers may still send
// reason 1; the sender also treats an abort before the first FileAck
// as a decline.
inline constexpr uint8_t kAbortRejected = 5;

// Sender trailer with the SHA-256 of the file bytes. Sent after the
// last FileChunk on the plaintext path so hashing can happen while
// the stream is already in flight. 32 raw digest bytes.
struct FileDigest {
	std::string transfer_id;
	std::vector<uint8_t> sha256;
};

// Folder transfer wrapper. The tree itself is in `sealed`
// (root name and empty directories). Each file is then a FileOffer
// whose sealed metadata carries this folder_id and a relative path.
struct FolderOffer {
	std::string folder_id; // 32 hex
	uint32_t file_count = 0;
	std::vector<uint8_t> sealed;
};

struct FolderAck {
	std::string folder_id;
	uint32_t acked_files = 0;
};

struct FolderComplete {
	std::string folder_id;
};

struct FolderAbort {
	std::string folder_id;
	uint8_t reason = 0;
};

// Group control and 1:N text. Confidential fields travel in `sealed`.
// group_id and msg_id stay in the clear so a receiver can route them.
struct GroupInvite {
	std::string group_id; // 32 hex
	std::vector<uint8_t> sealed;
};

struct GroupUpdate {
	std::string group_id;
	std::vector<uint8_t> sealed;
};

struct GroupText {
	std::string group_id;
	std::string msg_id; // 32 hex
	uint64_t timestamp_ms = 0;
	std::vector<uint8_t> sealed;
};

// Sender retracts a previously sent text or file (msg_id or transfer id).
// group_id is empty for 1:1 chats. The receiver only honors a recall
// from the original sender. The send window is kRecallWindowMs (48h).
struct Recall {
	std::string msg_id; // 32 hex
	std::string group_id; // empty or 32 hex
};

inline constexpr uint64_t kRecallWindowMs = 48ull * 60ull * 60ull * 1000ull;

inline bool within_recall_window(uint64_t sent_ms, uint64_t now_ms) {
	return sent_ms <= now_ms && (now_ms - sent_ms) <= kRecallWindowMs;
}

using Message = std::variant<
	Hello,
	Text,
	Ack,
	Bye,
	FileOffer,
	FileChunk,
	FileAck,
	FileComplete,
	FileAbort,
	FileDigest,
	FolderOffer,
	FolderAck,
	FolderComplete,
	FolderAbort,
	GroupInvite,
	GroupUpdate,
	GroupText,
	Recall>;

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
	// Official tde2e public key, 32 raw bytes.
	std::vector<uint8_t> public_key;
	std::string first_name;
	std::string last_name;
	std::string bio;
};

// UDP discovery datagram. Not length-prefixed; one datagram is one announce.
std::vector<uint8_t> encode_announce(const Announce &announce);
std::optional<Announce> decode_announce(const uint8_t *data, size_t size);

std::string random_hex_id();
std::string to_hex(const uint8_t *bytes, size_t size);
std::optional<std::vector<uint8_t>> from_hex(std::string_view hex);

} // namespace lan
