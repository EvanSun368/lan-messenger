#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace lan {

// Plaintext chunk size when file-transfer encryption is on. Leaves
// headroom in a 64KiB frame for the type header, transfer id, and
// official tde2e envelope expansion.
constexpr uint32_t kFileChunkPlainMax = 32 * 1024;
// Fast path when file-transfer encryption is off. Still fits under
// kMaxFileFramePayload after the FileChunk header is added.
// 2 MiB chunks plus a 32-chunk window (~64 MiB in flight) and sparse
// ACKs make the plaintext path closer to a LocalSend-style stream.
constexpr uint32_t kFileChunkPlainFast = 2 * 1024 * 1024;
constexpr uint32_t kFileWindow = 32;
constexpr uint32_t kPlainAckEvery = 8;
constexpr uint64_t kMaxFileSize = 16ull * 1024ull * 1024ull * 1024ull; // 16 GiB
constexpr size_t kSha256Size = 32;
constexpr size_t kMaxFileName = 255;
constexpr size_t kMaxRelativePath = 1024;
constexpr size_t kMaxFolderEntries = 10000;

using Sha256 = std::array<uint8_t, kSha256Size>;

// Incremental SHA-256 so a file can be hashed while it is streamed,
// instead of walking the whole file before the first byte is sent.
class Sha256Hasher {
public:
	Sha256Hasher();
	Sha256Hasher(Sha256Hasher &&) noexcept;
	Sha256Hasher &operator=(Sha256Hasher &&) noexcept;
	~Sha256Hasher();
	Sha256Hasher(const Sha256Hasher &) = delete;
	Sha256Hasher &operator=(const Sha256Hasher &) = delete;
	void reset();
	void update(const void *data, size_t size);
	// Hash the first `bytes` of a file (used to rebuild state on resume).
	void update_file_prefix(const std::filesystem::path &path, uint64_t bytes);
	[[nodiscard]] Sha256 finish();
	[[nodiscard]] bool finished() const;
private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};


struct FileOfferPlain {
	std::string filename;
	std::string mime;
	Sha256 sha256{};
	// Empty for a standalone 1:1 file. 32 hex when the file belongs to a folder.
	std::string folder_id;
	// Empty for a 1:1 transfer. 32 hex when the file was fanned out to a group.
	std::string group_id;
};

struct FolderOfferPlain {
	std::string root;
	uint32_t file_count = 0;
	std::vector<std::string> empty_dirs;
	// Empty for a 1:1 folder. 32 hex when fanned out to a group.
	std::string group_id;
};

struct FolderEntry {
	std::filesystem::path source;
	std::string relative;
	uint64_t size = 0;
};

struct FolderTree {
	std::string root;
	std::vector<FolderEntry> files;
	std::vector<std::string> empty_dirs;
	uint64_t total_size = 0;
};

// Disk snapshot of an in-progress transfer. Used to resume after a drop
// or a process crash. Abort deletes this record; pause keeps it.
struct TransferState {
	bool outgoing = false;
	std::string transfer_id;
	std::string peer_id;
	std::string folder_id;
	std::string group_id;
	std::filesystem::path path;
	std::string filename;
	std::string mime;
	uint64_t size = 0;
	Sha256 sha256{};
	uint32_t chunk_count = 0;
	uint32_t acked_count = 0;
	// 0 means kFileChunkPlainMax (legacy records).
	uint32_t chunk_size = 0;
	// Legacy records were always sealed with tde2e.
	bool encrypted = true;
	bool finished = false;
};

// Basename only, no controls or path separators, at most kMaxFileName bytes.
// Returns "file" if the result would otherwise be empty.
std::string sanitize_filename(std::string_view name);

uint32_t effective_chunk_size(uint32_t chunk_size);
uint32_t file_chunk_count(uint64_t size);
uint32_t file_chunk_count(uint64_t size, uint32_t chunk_size);
uint32_t file_chunk_plain_size(uint64_t size, uint32_t index);
uint32_t file_chunk_plain_size(uint64_t size, uint32_t index, uint32_t chunk_size);

Sha256 sha256_bytes(std::string_view data);
Sha256 sha256_file(const std::filesystem::path &path);

std::vector<uint8_t> encode_offer_plain(const FileOfferPlain &offer);
std::optional<FileOfferPlain> decode_offer_plain(
	const uint8_t *data,
	size_t size);

// Cleartext file/folder payload marker. A receiver that sees this magic
// accepts plaintext bytes; anything else is treated as a tde2e envelope
// and fails closed if decryption fails.
inline constexpr char kPlainPayloadMagic[4] = {'L', 'L', 'P', '1'};

bool is_plaintext_payload(const uint8_t *data, size_t size);
std::vector<uint8_t> wrap_plaintext_payload(const uint8_t *data, size_t size);
std::optional<std::vector<uint8_t>> unwrap_plaintext_payload(
	const uint8_t *data,
	size_t size);

// LLP1 + chunk_size + encode_offer_plain(...). Used when file encryption
// is off so the receiver knows the plaintext chunk size.
std::vector<uint8_t> encode_plain_file_offer(
	uint32_t chunk_size,
	const FileOfferPlain &offer);
struct PlainFileOffer {
	uint32_t chunk_size = 0;
	FileOfferPlain meta;
};
std::optional<PlainFileOffer> decode_plain_file_offer(
	const uint8_t *data,
	size_t size);

// Unique destination that does not overwrite an existing file.
std::filesystem::path unique_destination(
	const std::filesystem::path &dir,
	std::string_view filename);

// Relative path with '/' separators. Rejects empty, '.', '..', absolute
// paths, drive letters, and control characters. Each component is passed
// through sanitize_filename. Returns nullopt when the path is unsafe.
std::optional<std::string> sanitize_relative_path(std::string_view path);

std::vector<uint8_t> encode_folder_offer_plain(const FolderOfferPlain &offer);
std::optional<FolderOfferPlain> decode_folder_offer_plain(
	const uint8_t *data,
	size_t size);

// Walk a directory tree without following symlinks. Relative paths are
// sanitized. Empty directories are listed. Total size is capped at
// kMaxFileSize; more files than kMaxFolderEntries is an error.
std::optional<FolderTree> walk_folder(const std::filesystem::path &dir);

// Byte length of the first acked_count complete chunks.
uint64_t bytes_for_acked_chunks(uint64_t size, uint32_t acked_count);
uint64_t bytes_for_acked_chunks(uint64_t size, uint32_t acked_count, uint32_t chunk_size);
// Largest complete-chunk count covered by a partial file of part_size bytes.
uint32_t acked_count_from_part_size(uint64_t size, uint64_t part_size);
uint32_t acked_count_from_part_size(uint64_t size, uint64_t part_size, uint32_t chunk_size);
// True when size, digest, and peer identity all match. A finished or
// aborted record must not be treated as resumable.
bool transfer_can_resume(const TransferState &stored, const TransferState &offered);

std::vector<uint8_t> encode_transfer_state(const TransferState &state);
std::optional<TransferState> decode_transfer_state(
	const uint8_t *data,
	size_t size);
void save_transfer_state(
	const std::filesystem::path &dir,
	const TransferState &state);
std::optional<TransferState> load_transfer_state(
	const std::filesystem::path &dir,
	std::string_view transfer_id);
void remove_transfer_state(
	const std::filesystem::path &dir,
	std::string_view transfer_id);
std::vector<TransferState> load_all_transfer_states(
	const std::filesystem::path &dir);

// Unique directory under parent, appending " (n)" to the folder name.
std::filesystem::path unique_directory(
	const std::filesystem::path &parent,
	std::string_view name);

// Join root and a sanitized relative path. Returns nullopt if the result
// would escape root.
std::optional<std::filesystem::path> resolve_under_root(
	const std::filesystem::path &root,
	std::string_view relative);

// Safely delete a receiver-side file or folder after a recall.
// Only the exact path belonging to the transfer is removed. Symlinked
// directories, filesystem roots, and empty paths are refused.
enum class RecalledDeleteStatus {
	None,        // no path to delete
	Deleted,     // removed
	AlreadyGone, // missing (moved or already deleted)
	Locked,      // in use or permission denied
	Refused,     // unsafe / unrelated path
};

RecalledDeleteStatus delete_recalled_payload(
	const std::filesystem::path &path,
	bool is_folder);

} // namespace lan
