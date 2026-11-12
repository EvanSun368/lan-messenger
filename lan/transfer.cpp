#include "transfer.h"
#include "identity.h"
#include "protocol.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

#include <openssl/sha.h>

namespace lan {
namespace {

bool is_control(unsigned char ch) {
	return ch < 0x20 || ch == 0x7f;
}

std::string basename_of(std::string_view name) {
	size_t start = 0;
	for (size_t i = 0; i < name.size(); ++i) {
		if (name[i] == '/' || name[i] == '\\') {
			start = i + 1;
		}
	}
	return std::string(name.substr(start));
}

void append_u16be(std::vector<uint8_t> &out, uint16_t value) {
	out.push_back(static_cast<uint8_t>(value >> 8));
	out.push_back(static_cast<uint8_t>(value));
}

bool read_u16be(const uint8_t *&cursor, const uint8_t *end, uint16_t &value) {
	if (end - cursor < 2) {
		return false;
	}
	value = static_cast<uint16_t>((cursor[0] << 8) | cursor[1]);
	cursor += 2;
	return true;
}

void append_u8(std::vector<uint8_t> &out, uint8_t value) {
	out.push_back(value);
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

bool read_u8(const uint8_t *&cursor, const uint8_t *end, uint8_t &value) {
	if (cursor >= end) {
		return false;
	}
	value = *cursor++;
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

bool read_counted(const uint8_t *&cursor, const uint8_t *end, uint16_t max, std::string &out) {
	uint16_t len = 0;
	if (!read_u16be(cursor, end, len) || len > max) {
		return false;
	}
	if (static_cast<size_t>(end - cursor) < len) {
		return false;
	}
	out.assign(reinterpret_cast<const char *>(cursor), len);
	cursor += len;
	return true;
}

void append_counted(std::vector<uint8_t> &out, std::string_view value) {
	append_u16be(out, static_cast<uint16_t>(value.size()));
	out.insert(out.end(), value.begin(), value.end());
}

} // namespace

std::string sanitize_filename(std::string_view name) {
	auto base = basename_of(name);
	std::string out;
	out.reserve(base.size());
	for (unsigned char ch : base) {
		if (is_control(ch) || ch == '/' || ch == '\\') {
			continue;
		}
		out.push_back(static_cast<char>(ch));
	}
	while (!out.empty() && (out.back() == ' ' || out.back() == '.')) {
		out.pop_back();
	}
	if (out.size() > kMaxFileName) {
		out.resize(kMaxFileName);
		while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xc0) == 0x80) {
			out.pop_back();
		}
		if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xc0) == 0xc0) {
			out.pop_back();
		}
	}
	if (out.empty() || out == "." || out == "..") {
		return "file";
	}
	return out;
}

uint32_t effective_chunk_size(uint32_t chunk_size) {
	return chunk_size == 0 ? kFileChunkPlainMax : chunk_size;
}

uint32_t file_chunk_count(uint64_t size) {
	return file_chunk_count(size, kFileChunkPlainMax);
}

uint32_t file_chunk_count(uint64_t size, uint32_t chunk_size) {
	const auto step = effective_chunk_size(chunk_size);
	if (size == 0) {
		return 1; // one empty chunk so the transfer has a defined body
	}
	return static_cast<uint32_t>((size + step - 1) / step);
}

uint32_t file_chunk_plain_size(uint64_t size, uint32_t index) {
	return file_chunk_plain_size(size, index, kFileChunkPlainMax);
}

uint32_t file_chunk_plain_size(uint64_t size, uint32_t index, uint32_t chunk_size) {
	const auto step = effective_chunk_size(chunk_size);
	const auto count = file_chunk_count(size, step);
	if (index >= count) {
		return 0;
	}
	if (size == 0) {
		return 0;
	}
	if (index + 1 == count) {
		const auto rem = size % step;
		return rem == 0 ? step : static_cast<uint32_t>(rem);
	}
	return step;
}

Sha256 sha256_bytes(std::string_view data) {
	Sha256 out{};
	SHA256(
		reinterpret_cast<const unsigned char *>(data.data()),
		data.size(),
		out.data());
	return out;
}

Sha256 sha256_file(const std::filesystem::path &path) {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		throw std::runtime_error("unable to read " + path.string());
	}
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	char buffer[64 * 1024];
	while (in) {
		in.read(buffer, sizeof(buffer));
		const auto n = in.gcount();
		if (n > 0) {
			SHA256_Update(
				&ctx,
				reinterpret_cast<const unsigned char *>(buffer),
				static_cast<size_t>(n));
		}
	}
	if (!in.eof()) {
		throw std::runtime_error("unable to hash " + path.string());
	}
	Sha256 out{};
	SHA256_Final(out.data(), &ctx);
	return out;
}

struct Sha256Hasher::Impl {
	SHA256_CTX ctx{};
	bool finished = false;
	Sha256 digest{};
};

Sha256Hasher::Sha256Hasher()
	: impl_(std::make_unique<Impl>()) {
	reset();
}

Sha256Hasher::Sha256Hasher(Sha256Hasher &&) noexcept = default;
Sha256Hasher &Sha256Hasher::operator=(Sha256Hasher &&) noexcept = default;
Sha256Hasher::~Sha256Hasher() = default;

void Sha256Hasher::reset() {
	impl_->finished = false;
	impl_->digest = {};
	SHA256_Init(&impl_->ctx);
}

void Sha256Hasher::update(const void *data, size_t size) {
	if (impl_->finished) {
		throw std::runtime_error("sha256 hasher already finished");
	}
	if (data == nullptr || size == 0) {
		return;
	}
	SHA256_Update(&impl_->ctx, data, size);
}

void Sha256Hasher::update_file_prefix(const std::filesystem::path &path, uint64_t bytes) {
	if (bytes == 0) {
		return;
	}
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		throw std::runtime_error("unable to read " + path.string());
	}
	char buffer[64 * 1024];
	uint64_t left = bytes;
	while (left > 0 && in) {
		const auto n = std::min<uint64_t>(sizeof(buffer), left);
		in.read(buffer, static_cast<std::streamsize>(n));
		const auto got = in.gcount();
		if (got <= 0) {
			break;
		}
		update(buffer, static_cast<size_t>(got));
		left -= static_cast<uint64_t>(got);
	}
	if (left != 0) {
		throw std::runtime_error("unable to hash prefix of " + path.string());
	}
}

Sha256 Sha256Hasher::finish() {
	if (!impl_->finished) {
		SHA256_Final(impl_->digest.data(), &impl_->ctx);
		impl_->finished = true;
	}
	return impl_->digest;
}

bool Sha256Hasher::finished() const {
	return impl_->finished;
}

namespace {

void append_id16(std::vector<uint8_t> &out, const std::string &id) {
	if (id.empty()) {
		out.insert(out.end(), 16, 0);
		return;
	}
	const auto raw = from_hex(id);
	out.insert(out.end(), raw->begin(), raw->end());
}

bool id16_is_zero(const uint8_t *data) {
	for (size_t i = 0; i < 16; ++i) {
		if (data[i] != 0) {
			return false;
		}
	}
	return true;
}

} // namespace

std::vector<uint8_t> encode_offer_plain(const FileOfferPlain &offer) {
	if (offer.filename.size() > kMaxRelativePath || offer.mime.size() > 255) {
		throw std::invalid_argument("offer fields too large");
	}
	if (!offer.folder_id.empty() && !is_valid_peer_id(offer.folder_id)) {
		throw std::invalid_argument("invalid folder id");
	}
	if (!offer.group_id.empty() && !is_valid_peer_id(offer.group_id)) {
		throw std::invalid_argument("invalid group id");
	}
	std::vector<uint8_t> out;
	append_counted(out, offer.filename);
	append_counted(out, offer.mime);
	out.insert(out.end(), offer.sha256.begin(), offer.sha256.end());
	// v3-compatible: folder-only offers stay 16 trailing bytes.
	// Group offers use 32 trailing bytes (folder id, or zeros, then group id).
	if (!offer.group_id.empty()) {
		append_id16(out, offer.folder_id);
		append_id16(out, offer.group_id);
	} else if (!offer.folder_id.empty()) {
		append_id16(out, offer.folder_id);
	}
	return out;
}

std::optional<FileOfferPlain> decode_offer_plain(
	const uint8_t *data,
	size_t size) {
	if (data == nullptr) {
		return std::nullopt;
	}
	const uint8_t *cursor = data;
	const uint8_t *end = data + size;
	FileOfferPlain offer;
	if (!read_counted(cursor, end, static_cast<uint16_t>(kMaxRelativePath), offer.filename)
		|| !read_counted(cursor, end, 255, offer.mime)) {
		return std::nullopt;
	}
	if (static_cast<size_t>(end - cursor) < kSha256Size) {
		return std::nullopt;
	}
	std::copy(cursor, cursor + kSha256Size, offer.sha256.begin());
	cursor += kSha256Size;
	const auto remain = static_cast<size_t>(end - cursor);
	if (remain == 16) {
		offer.folder_id = to_hex(cursor, 16);
		cursor += 16;
		if (!is_valid_peer_id(offer.folder_id)) {
			return std::nullopt;
		}
	} else if (remain == 32) {
		if (!id16_is_zero(cursor)) {
			offer.folder_id = to_hex(cursor, 16);
			if (!is_valid_peer_id(offer.folder_id)) {
				return std::nullopt;
			}
		}
		cursor += 16;
		offer.group_id = to_hex(cursor, 16);
		cursor += 16;
		if (!is_valid_peer_id(offer.group_id)) {
			return std::nullopt;
		}
	} else if (remain != 0) {
		return std::nullopt;
	}
	const auto clean = sanitize_relative_path(offer.filename);
	if (!clean) {
		return std::nullopt;
	}
	offer.filename = *clean;
	return offer;
}

bool is_plaintext_payload(const uint8_t *data, size_t size) {
	return data != nullptr
		&& size >= 4
		&& data[0] == static_cast<uint8_t>(kPlainPayloadMagic[0])
		&& data[1] == static_cast<uint8_t>(kPlainPayloadMagic[1])
		&& data[2] == static_cast<uint8_t>(kPlainPayloadMagic[2])
		&& data[3] == static_cast<uint8_t>(kPlainPayloadMagic[3]);
}

std::vector<uint8_t> wrap_plaintext_payload(const uint8_t *data, size_t size) {
	std::vector<uint8_t> out;
	out.reserve(4 + size);
	out.insert(
		out.end(),
		reinterpret_cast<const uint8_t *>(kPlainPayloadMagic),
		reinterpret_cast<const uint8_t *>(kPlainPayloadMagic) + 4);
	if (data != nullptr && size > 0) {
		out.insert(out.end(), data, data + size);
	}
	return out;
}

std::optional<std::vector<uint8_t>> unwrap_plaintext_payload(
		const uint8_t *data,
		size_t size) {
	if (!is_plaintext_payload(data, size)) {
		return std::nullopt;
	}
	return std::vector<uint8_t>(data + 4, data + size);
}

std::vector<uint8_t> encode_plain_file_offer(
		uint32_t chunk_size,
		const FileOfferPlain &offer) {
	const auto step = effective_chunk_size(chunk_size);
	const auto meta = encode_offer_plain(offer);
	std::vector<uint8_t> inner;
	inner.push_back(static_cast<uint8_t>(step >> 24));
	inner.push_back(static_cast<uint8_t>(step >> 16));
	inner.push_back(static_cast<uint8_t>(step >> 8));
	inner.push_back(static_cast<uint8_t>(step));
	inner.insert(inner.end(), meta.begin(), meta.end());
	return wrap_plaintext_payload(inner.data(), inner.size());
}

std::optional<PlainFileOffer> decode_plain_file_offer(
		const uint8_t *data,
		size_t size) {
	const auto inner = unwrap_plaintext_payload(data, size);
	if (!inner || inner->size() < 4) {
		return std::nullopt;
	}
	PlainFileOffer offer;
	offer.chunk_size =
		(static_cast<uint32_t>((*inner)[0]) << 24)
		| (static_cast<uint32_t>((*inner)[1]) << 16)
		| (static_cast<uint32_t>((*inner)[2]) << 8)
		| static_cast<uint32_t>((*inner)[3]);
	if (offer.chunk_size == 0 || offer.chunk_size > kFileChunkPlainFast) {
		return std::nullopt;
	}
	const auto meta = decode_offer_plain(inner->data() + 4, inner->size() - 4);
	if (!meta) {
		return std::nullopt;
	}
	offer.meta = *meta;
	return offer;
}

std::filesystem::path unique_destination(
	const std::filesystem::path &dir,
	std::string_view filename) {
	const auto safe = sanitize_filename(filename);
	std::filesystem::create_directories(dir);
	auto dest = dir / safe;
	if (!std::filesystem::exists(dest)) {
		return dest;
	}
	const auto stem = dest.stem().string();
	const auto ext = dest.extension().string();
	for (int i = 1; i < 10000; ++i) {
		dest = dir / (stem + " (" + std::to_string(i) + ")" + ext);
		if (!std::filesystem::exists(dest)) {
			return dest;
		}
	}
	throw std::runtime_error("unable to pick a unique filename");
}

std::optional<std::string> sanitize_relative_path(std::string_view path) {
	if (path.empty() || path.size() > kMaxRelativePath) {
		return std::nullopt;
	}
	if (path[0] == '/' || path[0] == '\\') {
		return std::nullopt;
	}
	if (path.size() >= 2
		&& std::isalpha(static_cast<unsigned char>(path[0]))
		&& path[1] == ':') {
		return std::nullopt;
	}
	std::vector<std::string> parts;
	size_t start = 0;
	for (size_t i = 0; i <= path.size(); ++i) {
		if (i != path.size() && path[i] != '/' && path[i] != '\\') {
			continue;
		}
		const auto raw = path.substr(start, i - start);
		start = i + 1;
		if (raw.empty()) {
			if (i != path.size()) {
				return std::nullopt;
			}
			continue;
		}
		if (raw == "." || raw == "..") {
			return std::nullopt;
		}
		const auto clean = sanitize_filename(raw);
		if (clean.empty()) {
			return std::nullopt;
		}
		parts.push_back(clean);
	}
	if (parts.empty()) {
		return std::nullopt;
	}
	std::string out;
	for (size_t i = 0; i < parts.size(); ++i) {
		if (i != 0) {
			out.push_back('/');
		}
		out += parts[i];
	}
	if (out.size() > kMaxRelativePath) {
		return std::nullopt;
	}
	return out;
}

std::vector<uint8_t> encode_folder_offer_plain(const FolderOfferPlain &offer) {
	if (offer.root.size() > kMaxFileName
		|| offer.empty_dirs.size() > kMaxFolderEntries) {
		throw std::invalid_argument("folder offer too large");
	}
	std::vector<uint8_t> out;
	append_counted(out, offer.root);
	append_u32be(out, offer.file_count);
	append_u32be(out, static_cast<uint32_t>(offer.empty_dirs.size()));
	for (const auto &dir : offer.empty_dirs) {
		if (dir.size() > kMaxRelativePath) {
			throw std::invalid_argument("folder path too long");
		}
		append_counted(out, dir);
	}
	if (!offer.group_id.empty()) {
		if (!is_valid_peer_id(offer.group_id)) {
			throw std::invalid_argument("invalid group id");
		}
		append_id16(out, offer.group_id);
	}
	return out;
}

std::optional<FolderOfferPlain> decode_folder_offer_plain(
	const uint8_t *data,
	size_t size) {
	if (data == nullptr) {
		return std::nullopt;
	}
	const uint8_t *cursor = data;
	const uint8_t *end = data + size;
	FolderOfferPlain offer;
	std::string root;
	uint32_t dir_count = 0;
	if (!read_counted(cursor, end, static_cast<uint16_t>(kMaxFileName), root)
		|| !read_u32be(cursor, end, offer.file_count)
		|| !read_u32be(cursor, end, dir_count)
		|| dir_count > kMaxFolderEntries) {
		return std::nullopt;
	}
	offer.root = sanitize_filename(root);
	if (offer.root.empty()) {
		return std::nullopt;
	}
	offer.empty_dirs.reserve(dir_count);
	for (uint32_t i = 0; i < dir_count; ++i) {
		std::string dir;
		if (!read_counted(cursor, end, static_cast<uint16_t>(kMaxRelativePath), dir)) {
			return std::nullopt;
		}
		const auto clean = sanitize_relative_path(dir);
		if (!clean) {
			return std::nullopt;
		}
		offer.empty_dirs.push_back(*clean);
	}
	const auto remain = static_cast<size_t>(end - cursor);
	if (remain == 16) {
		offer.group_id = to_hex(cursor, 16);
		cursor += 16;
		if (!is_valid_peer_id(offer.group_id)) {
			return std::nullopt;
		}
	} else if (remain != 0) {
		return std::nullopt;
	}
	return offer;
}

bool file_is_under_dir(
		const std::string &file,
		const std::string &dir) {
	return file.size() > dir.size()
		&& file.compare(0, dir.size(), dir) == 0
		&& file[dir.size()] == '/';
}

std::optional<FolderTree> walk_folder(const std::filesystem::path &dir) {
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec)) {
		return std::nullopt;
	}
	FolderTree tree;
	tree.root = sanitize_filename(dir.filename().u8string());
	if (tree.root.empty()) {
		tree.root = "folder";
	}
	const auto options = std::filesystem::directory_options::skip_permission_denied;
	std::vector<std::string> all_dirs;
	for (auto it = std::filesystem::recursive_directory_iterator(dir, options, ec);
		it != std::filesystem::recursive_directory_iterator();
		it.increment(ec)) {
		if (ec) {
			return std::nullopt;
		}
		const auto &entry = *it;
		if (entry.is_symlink(ec)) {
			continue;
		}
		const auto rel = std::filesystem::relative(entry.path(), dir, ec);
		if (ec) {
			return std::nullopt;
		}
		const auto clean = sanitize_relative_path(rel.generic_u8string());
		if (!clean) {
			return std::nullopt;
		}
		if (entry.is_directory(ec)) {
			all_dirs.push_back(*clean);
			continue;
		}
		if (!entry.is_regular_file(ec)) {
			continue;
		}
		const auto size = entry.file_size(ec);
		if (ec || size > kMaxFileSize) {
			return std::nullopt;
		}
		if (tree.files.size() >= kMaxFolderEntries) {
			return std::nullopt;
		}
		tree.total_size += size;
		if (tree.total_size > kMaxFileSize) {
			return std::nullopt;
		}
		FolderEntry file;
		file.source = entry.path();
		file.relative = *clean;
		file.size = size;
		tree.files.push_back(std::move(file));
	}
	for (const auto &empty : all_dirs) {
		bool has_file = false;
		for (const auto &file : tree.files) {
			if (file_is_under_dir(file.relative, empty)) {
				has_file = true;
				break;
			}
		}
		if (!has_file) {
			tree.empty_dirs.push_back(empty);
		}
	}
	return tree;
}

uint64_t bytes_for_acked_chunks(uint64_t size, uint32_t acked_count) {
	return bytes_for_acked_chunks(size, acked_count, kFileChunkPlainMax);
}

uint64_t bytes_for_acked_chunks(uint64_t size, uint32_t acked_count, uint32_t chunk_size) {
	const auto step = effective_chunk_size(chunk_size);
	if (acked_count == 0) {
		return 0;
	}
	if (acked_count >= file_chunk_count(size, step)) {
		return size;
	}
	return uint64_t(acked_count) * step;
}

uint32_t acked_count_from_part_size(uint64_t size, uint64_t part_size) {
	return acked_count_from_part_size(size, part_size, kFileChunkPlainMax);
}

uint32_t acked_count_from_part_size(uint64_t size, uint64_t part_size, uint32_t chunk_size) {
	const auto step = effective_chunk_size(chunk_size);
	if (part_size == 0 || size == 0) {
		return 0;
	}
	if (part_size >= size) {
		return file_chunk_count(size, step);
	}
	return static_cast<uint32_t>(part_size / step);
}

bool transfer_can_resume(const TransferState &stored, const TransferState &offered) {
	if (stored.finished || offered.finished) {
		return false;
	}
	if (stored.transfer_id.empty()
		|| stored.transfer_id != offered.transfer_id
		|| stored.peer_id.empty()
		|| stored.peer_id != offered.peer_id
		|| stored.size != offered.size
		|| stored.folder_id != offered.folder_id) {
		return false;
	}
	if (stored.sha256 != offered.sha256
		&& stored.sha256 != Sha256{}
		&& offered.sha256 != Sha256{}) {
		return false;
	}
	if (offered.chunk_count != 0 && stored.chunk_count != offered.chunk_count) {
		return false;
	}
	const auto step = effective_chunk_size(stored.chunk_size);
	if (effective_chunk_size(offered.chunk_size) != step) {
		return false;
	}
	if (stored.encrypted != offered.encrypted) {
		return false;
	}
	return stored.chunk_count == file_chunk_count(stored.size, step);
}

std::vector<uint8_t> encode_transfer_state(const TransferState &state) {
	if (!is_valid_peer_id(state.transfer_id)
		|| !is_valid_peer_id(state.peer_id)
		|| (!state.folder_id.empty() && !is_valid_peer_id(state.folder_id))
		|| (!state.group_id.empty() && !is_valid_peer_id(state.group_id))
		|| state.filename.size() > kMaxRelativePath
		|| state.mime.size() > 255) {
		throw std::invalid_argument("invalid transfer state");
	}
	const auto path = state.path.generic_u8string();
	if (path.size() > 4096) {
		throw std::invalid_argument("transfer path too long");
	}
	std::vector<uint8_t> out;
	append_u8(out, 3);
	append_u8(out, state.outgoing ? 1 : 0);
	append_u8(out, state.finished ? 1 : 0);
	const auto transfer_raw = from_hex(state.transfer_id);
	const auto peer_raw = from_hex(state.peer_id);
	out.insert(out.end(), transfer_raw->begin(), transfer_raw->end());
	out.insert(out.end(), peer_raw->begin(), peer_raw->end());
	append_u8(out, state.folder_id.empty() ? 0 : 1);
	if (!state.folder_id.empty()) {
		const auto folder_raw = from_hex(state.folder_id);
		out.insert(out.end(), folder_raw->begin(), folder_raw->end());
	}
	append_u8(out, state.group_id.empty() ? 0 : 1);
	if (!state.group_id.empty()) {
		append_id16(out, state.group_id);
	}
	append_counted(out, path);
	append_counted(out, state.filename);
	append_counted(out, state.mime);
	append_u64be(out, state.size);
	out.insert(out.end(), state.sha256.begin(), state.sha256.end());
	append_u32be(out, state.chunk_count);
	append_u32be(out, state.acked_count);
	append_u32be(out, effective_chunk_size(state.chunk_size));
	append_u8(out, state.encrypted ? 1 : 0);
	return out;
}

std::optional<TransferState> decode_transfer_state(
	const uint8_t *data,
	size_t size) {
	if (data == nullptr) {
		return std::nullopt;
	}
	const uint8_t *cursor = data;
	const uint8_t *end = data + size;
	TransferState state;
	uint8_t version = 0;
	uint8_t outgoing = 0;
	uint8_t finished = 0;
	uint8_t has_folder = 0;
	if (!read_u8(cursor, end, version)
		|| (version != 1 && version != 2 && version != 3)
		|| !read_u8(cursor, end, outgoing)
		|| !read_u8(cursor, end, finished)
		|| static_cast<size_t>(end - cursor) < 32) {
		return std::nullopt;
	}
	state.outgoing = outgoing == 1;
	state.finished = finished == 1;
	state.transfer_id = to_hex(cursor, 16);
	cursor += 16;
	state.peer_id = to_hex(cursor, 16);
	cursor += 16;
	if (!is_valid_peer_id(state.transfer_id) || !is_valid_peer_id(state.peer_id)) {
		return std::nullopt;
	}
	if (!read_u8(cursor, end, has_folder)) {
		return std::nullopt;
	}
	if (has_folder == 1) {
		if (static_cast<size_t>(end - cursor) < 16) {
			return std::nullopt;
		}
		state.folder_id = to_hex(cursor, 16);
		cursor += 16;
		if (!is_valid_peer_id(state.folder_id)) {
			return std::nullopt;
		}
	} else if (has_folder != 0) {
		return std::nullopt;
	}
	if (version >= 2) {
		uint8_t has_group = 0;
		if (!read_u8(cursor, end, has_group)) {
			return std::nullopt;
		}
		if (has_group == 1) {
			if (static_cast<size_t>(end - cursor) < 16) {
				return std::nullopt;
			}
			state.group_id = to_hex(cursor, 16);
			cursor += 16;
			if (!is_valid_peer_id(state.group_id)) {
				return std::nullopt;
			}
		} else if (has_group != 0) {
			return std::nullopt;
		}
	}
	std::string path;
	if (!read_counted(cursor, end, 4096, path)
		|| !read_counted(cursor, end, static_cast<uint16_t>(kMaxRelativePath), state.filename)
		|| !read_counted(cursor, end, 255, state.mime)
		|| !read_u64be(cursor, end, state.size)
		|| static_cast<size_t>(end - cursor) < kSha256Size) {
		return std::nullopt;
	}
	state.path = std::filesystem::u8path(path);
	std::copy(cursor, cursor + kSha256Size, state.sha256.begin());
	cursor += kSha256Size;
	if (!read_u32be(cursor, end, state.chunk_count)
		|| !read_u32be(cursor, end, state.acked_count)) {
		return std::nullopt;
	}
	if (version >= 3) {
		uint8_t encrypted = 0;
		if (!read_u32be(cursor, end, state.chunk_size)
			|| state.chunk_size == 0
			|| !read_u8(cursor, end, encrypted)
			|| (encrypted != 0 && encrypted != 1)
			|| cursor != end) {
			return std::nullopt;
		}
		state.encrypted = encrypted == 1;
	} else {
		state.chunk_size = kFileChunkPlainMax;
		state.encrypted = true;
		if (cursor != end) {
			return std::nullopt;
		}
	}
	return state;
}

std::filesystem::path state_path(
	const std::filesystem::path &dir,
	std::string_view transfer_id) {
	return dir / (std::string(transfer_id) + ".state");
}

void save_transfer_state(
	const std::filesystem::path &dir,
	const TransferState &state) {
	std::filesystem::create_directories(dir);
	const auto encoded = encode_transfer_state(state);
	const auto dest = state_path(dir, state.transfer_id);
	const auto tmp = dest.string() + ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		out.write(
			reinterpret_cast<const char *>(encoded.data()),
			static_cast<std::streamsize>(encoded.size()));
		if (!out) {
			throw std::runtime_error("unable to write transfer state");
		}
	}
	std::filesystem::rename(tmp, dest);
}

std::optional<TransferState> load_transfer_state(
	const std::filesystem::path &dir,
	std::string_view transfer_id) {
	const auto path = state_path(dir, transfer_id);
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return std::nullopt;
	}
	std::vector<uint8_t> data(
		(std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
	return decode_transfer_state(data.data(), data.size());
}

void remove_transfer_state(
	const std::filesystem::path &dir,
	std::string_view transfer_id) {
	std::error_code ec;
	std::filesystem::remove(state_path(dir, transfer_id), ec);
}

std::vector<TransferState> load_all_transfer_states(
	const std::filesystem::path &dir) {
	std::vector<TransferState> out;
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec)) {
		return out;
	}
	for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
		if (ec || !entry.is_regular_file(ec) || entry.path().extension() != ".state") {
			continue;
		}
		std::ifstream in(entry.path(), std::ios::binary);
		if (!in) {
			continue;
		}
		std::vector<uint8_t> data(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());
		auto state = decode_transfer_state(data.data(), data.size());
		if (state) {
			out.push_back(std::move(*state));
		}
	}
	return out;
}

std::filesystem::path unique_directory(
	const std::filesystem::path &parent,
	std::string_view name) {
	const auto safe = sanitize_filename(name);
	std::filesystem::create_directories(parent);
	auto dest = parent / safe;
	if (!std::filesystem::exists(dest)) {
		return dest;
	}
	for (int i = 1; i < 10000; ++i) {
		dest = parent / (safe + " (" + std::to_string(i) + ")");
		if (!std::filesystem::exists(dest)) {
			return dest;
		}
	}
	throw std::runtime_error("unable to pick a unique directory");
}

std::optional<std::filesystem::path> resolve_under_root(
	const std::filesystem::path &root,
	std::string_view relative) {
	const auto clean = sanitize_relative_path(relative);
	if (!clean) {
		return std::nullopt;
	}
	auto dest = root;
	size_t start = 0;
	const auto &path = *clean;
	for (size_t i = 0; i <= path.size(); ++i) {
		if (i != path.size() && path[i] != '/') {
			continue;
		}
		dest /= path.substr(start, i - start);
		start = i + 1;
	}
	const auto normal_root = root.lexically_normal();
	const auto normal_dest = dest.lexically_normal();
	const auto rel = normal_dest.lexically_relative(normal_root);
	if (rel.empty() || rel == std::filesystem::path(".")) {
		return std::nullopt;
	}
	for (const auto &part : rel) {
		if (part == "..") {
			return std::nullopt;
		}
	}
	return dest;
}


bool is_filesystem_root(const std::filesystem::path &path) {
	const auto normal = path.lexically_normal();
	return !normal.has_relative_path() || normal == normal.root_path();
}

RecalledDeleteStatus delete_recalled_payload(
		const std::filesystem::path &path,
		bool is_folder) {
	if (path.empty()) {
		return RecalledDeleteStatus::None;
	}
	std::error_code ec;
	const auto normal = path.lexically_normal();
	if (normal.empty() || is_filesystem_root(normal)) {
		return RecalledDeleteStatus::Refused;
	}
	// Refuse obviously dangerous destinations.
	if (normal.filename().empty() && !is_folder) {
		return RecalledDeleteStatus::Refused;
	}
	const auto status = std::filesystem::symlink_status(normal, ec);
	if (ec) {
		if (ec == std::errc::no_such_file_or_directory) {
			return RecalledDeleteStatus::AlreadyGone;
		}
		return RecalledDeleteStatus::Locked;
	}
	if (!std::filesystem::exists(status)) {
		return RecalledDeleteStatus::AlreadyGone;
	}
	if (is_folder) {
		if (std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
			return RecalledDeleteStatus::Refused;
		}
		const auto count = std::filesystem::remove_all(normal, ec);
		if (!ec && count > 0) {
			return RecalledDeleteStatus::Deleted;
		}
		if (ec == std::errc::no_such_file_or_directory) {
			return RecalledDeleteStatus::AlreadyGone;
		}
		return RecalledDeleteStatus::Locked;
	}
	if (std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status)) {
		return RecalledDeleteStatus::Refused;
	}
	if (!std::filesystem::remove(normal, ec)) {
		if (ec == std::errc::no_such_file_or_directory || !ec) {
			return RecalledDeleteStatus::AlreadyGone;
		}
		return RecalledDeleteStatus::Locked;
	}
	return RecalledDeleteStatus::Deleted;
}

} // namespace lan
