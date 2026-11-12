#include "check.h"
#include "transfer.h"
#include "e2e.h"
#include "protocol.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::filesystem::path make_temp_dir() {
	auto path = std::filesystem::temp_directory_path()
		/ ("lanlink-xfer-" + std::to_string(getpid()) + "-" + std::to_string(rand()));
	std::filesystem::create_directories(path);
	return path;
}

void write_file(const std::filesystem::path &path, std::string_view body) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(body.data(), static_cast<std::streamsize>(body.size()));
}

} // namespace

LAN_TEST(sanitize_filename_strips_paths_and_controls) {
	LAN_CHECK_EQ(lan::sanitize_filename("hello.txt"), std::string("hello.txt"));
	LAN_CHECK_EQ(lan::sanitize_filename("/tmp/../etc/passwd"), std::string("passwd"));
	LAN_CHECK_EQ(lan::sanitize_filename("C:\\Windows\\a b.txt"), std::string("a b.txt"));
	LAN_CHECK_EQ(lan::sanitize_filename(".."), std::string("file"));
	LAN_CHECK_EQ(lan::sanitize_filename(""), std::string("file"));
	LAN_CHECK_EQ(lan::sanitize_filename("bad\nname"), std::string("badname"));
	LAN_CHECK(lan::sanitize_filename(std::string(300, 'a')).size() == lan::kMaxFileName);
}

LAN_TEST(chunk_count_covers_the_whole_file) {
	LAN_CHECK_EQ(lan::file_chunk_count(0), uint32_t(1));
	LAN_CHECK_EQ(lan::file_chunk_plain_size(0, 0), uint32_t(0));
	LAN_CHECK_EQ(lan::file_chunk_count(lan::kFileChunkPlainMax), uint32_t(1));
	LAN_CHECK_EQ(lan::file_chunk_plain_size(lan::kFileChunkPlainMax, 0), lan::kFileChunkPlainMax);
	LAN_CHECK_EQ(lan::file_chunk_count(lan::kFileChunkPlainMax + 1), uint32_t(2));
	LAN_CHECK_EQ(lan::file_chunk_plain_size(lan::kFileChunkPlainMax + 1, 0), lan::kFileChunkPlainMax);
	LAN_CHECK_EQ(lan::file_chunk_plain_size(lan::kFileChunkPlainMax + 1, 1), uint32_t(1));
	uint64_t covered = 0;
	const uint64_t size = lan::kFileChunkPlainMax * 3 + 17;
	const auto count = lan::file_chunk_count(size);
	for (uint32_t i = 0; i < count; ++i) {
		covered += lan::file_chunk_plain_size(size, i);
	}
	LAN_CHECK_EQ(covered, size);
}

LAN_TEST(sha256_file_matches_bytes_and_is_stable) {
	const auto dir = make_temp_dir();
	const auto path = dir / "payload.bin";
	const std::string body = "abc";
	write_file(path, body);
	const auto from_bytes = lan::sha256_bytes(body);
	const auto from_file = lan::sha256_file(path);
	LAN_CHECK(from_bytes == from_file);
	// Known SHA-256("abc")
	const auto hex = lan::to_hex(from_bytes.data(), from_bytes.size());
	LAN_CHECK_EQ(
		hex,
		std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
	LAN_CHECK(lan::sha256_bytes("abc") != lan::sha256_bytes("abd"));
	std::filesystem::remove_all(dir);
}

LAN_TEST(offer_plain_roundtrip_and_sanitizes_name) {
	lan::FileOfferPlain offer;
	offer.filename = "shop/发票.pdf";
	offer.mime = "application/pdf";
	offer.sha256 = lan::sha256_bytes("data");
	const auto encoded = lan::encode_offer_plain(offer);
	const auto decoded = lan::decode_offer_plain(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->filename, std::string("shop/发票.pdf"));
	LAN_CHECK_EQ(decoded->mime, std::string("application/pdf"));
	LAN_CHECK(decoded->sha256 == offer.sha256);
	LAN_CHECK(decoded->folder_id.empty());
	LAN_CHECK(!lan::decode_offer_plain(encoded.data(), 3).has_value());
}

LAN_TEST(unique_destination_does_not_overwrite) {
	const auto dir = make_temp_dir();
	const auto first = lan::unique_destination(dir, "note.txt");
	write_file(first, "one");
	const auto second = lan::unique_destination(dir, "note.txt");
	LAN_CHECK(first != second);
	LAN_CHECK_EQ(second.filename().string(), std::string("note (1).txt"));
	std::filesystem::remove_all(dir);
}

LAN_TEST(encrypted_file_chunk_fits_in_max_frame) {
	const auto alice = lan::E2EKey::generate();
	const auto bob = lan::E2EKey::generate();
	const std::string plain(lan::kFileChunkPlainMax, 'x');
	const auto sealed = alice.encrypt_for(bob.public_key(), plain);
	lan::FileChunk chunk;
	chunk.transfer_id = lan::random_hex_id();
	chunk.index = 0;
	chunk.sealed = sealed;
	const auto frame = lan::encode_frame(chunk);
	LAN_CHECK(frame.size() <= 4 + lan::kMaxFramePayload);
	const auto decoded = lan::try_decode_frame(frame.data(), frame.size());
	LAN_REQUIRE(decoded.has_value());
	const auto *again = std::get_if<lan::FileChunk>(&decoded->message);
	LAN_REQUIRE(again != nullptr);
	const auto opened = bob.decrypt_from(
		alice.public_key(),
		std::string_view(
			reinterpret_cast<const char *>(again->sealed.data()),
			again->sealed.size()));
	LAN_REQUIRE(opened.has_value());
	LAN_CHECK_EQ(*opened, plain);
	// Fail closed: the ciphertext must not contain the plaintext.
	const std::string view(frame.begin(), frame.end());
	LAN_CHECK(view.find(plain.substr(0, 64)) == std::string::npos);
}

LAN_TEST(file_offer_is_fail_closed_without_the_peer_key) {
	const auto alice = lan::E2EKey::generate();
	const auto bob = lan::E2EKey::generate();
	const auto eve = lan::E2EKey::generate();
	lan::FileOfferPlain meta;
	meta.filename = "secret.txt";
	meta.mime = "text/plain";
	meta.sha256 = lan::sha256_bytes("secret");
	const auto plain = lan::encode_offer_plain(meta);
	const auto sealed = alice.encrypt_for(
		bob.public_key(),
		std::string_view(reinterpret_cast<const char *>(plain.data()), plain.size()));
	lan::FileOffer offer;
	offer.transfer_id = lan::random_hex_id();
	offer.size = 6;
	offer.chunk_count = 1;
	offer.sealed = sealed;
	const auto frame = lan::encode_frame(offer);
	const auto decoded = lan::try_decode_frame(frame.data(), frame.size());
	LAN_REQUIRE(decoded.has_value());
	const auto *got = std::get_if<lan::FileOffer>(&decoded->message);
	LAN_REQUIRE(got != nullptr);
	const std::string sealed_view(
		reinterpret_cast<const char *>(got->sealed.data()),
		got->sealed.size());
	LAN_CHECK(!eve.decrypt_from(alice.public_key(), sealed_view).has_value());
	LAN_CHECK(std::string(frame.begin(), frame.end()).find("secret.txt")
		== std::string::npos);
	const auto opened = bob.decrypt_from(alice.public_key(), sealed_view);
	LAN_REQUIRE(opened.has_value());
	const auto meta_back = lan::decode_offer_plain(
		reinterpret_cast<const uint8_t *>(opened->data()),
		opened->size());
	LAN_REQUIRE(meta_back.has_value());
	LAN_CHECK_EQ(meta_back->filename, std::string("secret.txt"));
}

LAN_TEST(sanitize_relative_path_rejects_escapes_and_keeps_tree) {
	const auto ok = lan::sanitize_relative_path("docs/a b/发票.pdf");
	LAN_REQUIRE(ok.has_value());
	LAN_CHECK_EQ(*ok, std::string("docs/a b/发票.pdf"));
	LAN_CHECK_EQ(*lan::sanitize_relative_path("hello.txt"), std::string("hello.txt"));
	const auto win = lan::sanitize_relative_path("win\\sub\\file.txt");
	LAN_REQUIRE(win.has_value());
	LAN_CHECK_EQ(*win, std::string("win/sub/file.txt"));
	LAN_CHECK(!lan::sanitize_relative_path("").has_value());
	LAN_CHECK(!lan::sanitize_relative_path("..").has_value());
	LAN_CHECK(!lan::sanitize_relative_path("../etc/passwd").has_value());
	LAN_CHECK(!lan::sanitize_relative_path("docs/../secret").has_value());
	LAN_CHECK(!lan::sanitize_relative_path("/tmp/abs").has_value());
	LAN_CHECK(!lan::sanitize_relative_path("C:\\\\Windows\\\\a.txt").has_value());
	LAN_CHECK(!lan::sanitize_relative_path("c:/Windows/a.txt").has_value());
	LAN_CHECK(!lan::sanitize_relative_path("./hidden").has_value());
}

LAN_TEST(resolve_under_root_stays_inside_the_folder) {
	const auto root = std::filesystem::path("/tmp/lanlink-root");
	const auto dest = lan::resolve_under_root(root, "docs/readme.txt");
	LAN_REQUIRE(dest.has_value());
	LAN_CHECK_EQ(dest->generic_string(), std::string("/tmp/lanlink-root/docs/readme.txt"));
	LAN_CHECK(!lan::resolve_under_root(root, "../escape.txt").has_value());
	LAN_CHECK(!lan::resolve_under_root(root, "/abs.txt").has_value());
	LAN_CHECK(!lan::resolve_under_root(root, "").has_value());
}

LAN_TEST(walk_folder_preserves_tree_and_empty_dirs) {
	const auto dir = make_temp_dir() / "Project";
	std::filesystem::create_directories(dir / "src" / "util");
	std::filesystem::create_directories(dir / "empty" / "nested");
	write_file(dir / "src" / "main.cpp", "int main(){}");
	write_file(dir / "src" / "util" / "log.h", "log");
	write_file(dir / "README.md", "hi");
	const auto tree = lan::walk_folder(dir);
	LAN_REQUIRE(tree.has_value());
	LAN_CHECK_EQ(tree->root, std::string("Project"));
	LAN_CHECK_EQ(tree->files.size(), size_t(3));
	LAN_CHECK_EQ(tree->total_size, uint64_t(12 + 3 + 2));
	std::vector<std::string> rels;
	for (const auto &file : tree->files) {
		rels.push_back(file.relative);
	}
	std::sort(rels.begin(), rels.end());
	LAN_CHECK_EQ(rels[0], std::string("README.md"));
	LAN_CHECK_EQ(rels[1], std::string("src/main.cpp"));
	LAN_CHECK_EQ(rels[2], std::string("src/util/log.h"));
	LAN_REQUIRE(tree->empty_dirs.size() >= 1);
	bool saw_empty = false;
	for (const auto &empty : tree->empty_dirs) {
		if (empty == "empty/nested" || empty == "empty") {
			saw_empty = true;
		}
		for (const auto &file : tree->files) {
			LAN_CHECK(file.relative.find(empty + "/") != 0);
		}
	}
	LAN_CHECK(saw_empty);
	LAN_CHECK(!lan::walk_folder(dir / "missing").has_value());
	std::filesystem::remove_all(dir.parent_path());
}

LAN_TEST(folder_offer_plain_roundtrip) {
	lan::FolderOfferPlain offer;
	offer.root = "Project";
	offer.file_count = 3;
	offer.empty_dirs = {"empty/nested"};
	const auto encoded = lan::encode_folder_offer_plain(offer);
	const auto decoded = lan::decode_folder_offer_plain(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->root, std::string("Project"));
	LAN_CHECK_EQ(decoded->file_count, uint32_t(3));
	LAN_REQUIRE(decoded->empty_dirs.size() == 1);
	LAN_CHECK_EQ(decoded->empty_dirs[0], std::string("empty/nested"));
	LAN_CHECK(!lan::decode_folder_offer_plain(encoded.data(), 2).has_value());
}

LAN_TEST(offer_plain_keeps_folder_id_and_relative_path) {
	lan::FileOfferPlain offer;
	offer.filename = "src/main.cpp";
	offer.mime = "text/x-c++src";
	offer.sha256 = lan::sha256_bytes("int");
	offer.folder_id = lan::random_hex_id();
	const auto encoded = lan::encode_offer_plain(offer);
	const auto decoded = lan::decode_offer_plain(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->filename, std::string("src/main.cpp"));
	LAN_CHECK_EQ(decoded->folder_id, offer.folder_id);
	lan::FileOfferPlain bad = offer;
	bad.filename = "../secret";
	const auto bad_encoded = lan::encode_offer_plain(bad);
	LAN_CHECK(!lan::decode_offer_plain(bad_encoded.data(), bad_encoded.size()).has_value());
}

LAN_TEST(resume_offset_math_uses_complete_chunks) {
	const uint64_t size = lan::kFileChunkPlainMax * 2 + 10;
	LAN_CHECK_EQ(lan::bytes_for_acked_chunks(size, 0), uint64_t(0));
	LAN_CHECK_EQ(lan::bytes_for_acked_chunks(size, 1), uint64_t(lan::kFileChunkPlainMax));
	LAN_CHECK_EQ(lan::bytes_for_acked_chunks(size, 2), uint64_t(lan::kFileChunkPlainMax) * 2);
	LAN_CHECK_EQ(lan::bytes_for_acked_chunks(size, 3), size);
	LAN_CHECK_EQ(lan::acked_count_from_part_size(size, 0), uint32_t(0));
	LAN_CHECK_EQ(lan::acked_count_from_part_size(size, lan::kFileChunkPlainMax + 9), uint32_t(1));
	LAN_CHECK_EQ(lan::acked_count_from_part_size(size, size), lan::file_chunk_count(size));
	LAN_CHECK_EQ(lan::bytes_for_acked_chunks(0, 1), uint64_t(0));
	LAN_CHECK_EQ(lan::acked_count_from_part_size(0, 0), uint32_t(0));
}

LAN_TEST(transfer_can_resume_requires_matching_identity) {
	lan::TransferState stored;
	stored.transfer_id = lan::random_hex_id();
	stored.peer_id = lan::random_hex_id();
	stored.size = 100;
	stored.sha256 = lan::sha256_bytes("abc");
	stored.chunk_count = lan::file_chunk_count(100);
	auto offered = stored;
	LAN_CHECK(lan::transfer_can_resume(stored, offered));
	offered.size = 99;
	LAN_CHECK(!lan::transfer_can_resume(stored, offered));
	offered = stored;
	offered.sha256 = lan::sha256_bytes("abd");
	LAN_CHECK(!lan::transfer_can_resume(stored, offered));
	offered = stored;
	offered.peer_id = lan::random_hex_id();
	LAN_CHECK(!lan::transfer_can_resume(stored, offered));
	offered = stored;
	stored.finished = true;
	LAN_CHECK(!lan::transfer_can_resume(stored, offered));
}

LAN_TEST(transfer_state_roundtrip_and_persist) {
	const auto dir = make_temp_dir();
	lan::TransferState state;
	state.outgoing = true;
	state.transfer_id = lan::random_hex_id();
	state.peer_id = lan::random_hex_id();
	state.folder_id = lan::random_hex_id();
	state.path = dir / "src" / "main.cpp";
	state.filename = "src/main.cpp";
	state.mime = "text/plain";
	state.size = 42;
	state.sha256 = lan::sha256_bytes("payload");
	state.chunk_count = 1;
	state.acked_count = 0;
	lan::save_transfer_state(dir, state);
	const auto loaded = lan::load_transfer_state(dir, state.transfer_id);
	LAN_REQUIRE(loaded.has_value());
	LAN_CHECK(loaded->outgoing);
	LAN_CHECK_EQ(loaded->peer_id, state.peer_id);
	LAN_CHECK_EQ(loaded->folder_id, state.folder_id);
	LAN_CHECK_EQ(loaded->filename, std::string("src/main.cpp"));
	LAN_CHECK_EQ(loaded->size, uint64_t(42));
	LAN_CHECK(loaded->sha256 == state.sha256);
	const auto all = lan::load_all_transfer_states(dir);
	LAN_CHECK_EQ(all.size(), size_t(1));
	lan::remove_transfer_state(dir, state.transfer_id);
	LAN_CHECK(!lan::load_transfer_state(dir, state.transfer_id).has_value());
	std::filesystem::remove_all(dir);
}

LAN_TEST(unique_directory_does_not_overwrite) {
	const auto dir = make_temp_dir();
	const auto first = lan::unique_directory(dir, "Project");
	std::filesystem::create_directories(first);
	const auto second = lan::unique_directory(dir, "Project");
	LAN_CHECK(first != second);
	LAN_CHECK_EQ(second.filename().string(), std::string("Project (1)"));
	std::filesystem::remove_all(dir);
}

LAN_TEST(offer_plain_keeps_group_id_and_legacy_folder_only) {
	lan::FileOfferPlain offer;
	offer.filename = "src/main.cpp";
	offer.mime = "text/plain";
	offer.sha256 = lan::sha256_bytes("int");
	offer.group_id = lan::random_hex_id();
	const auto encoded = lan::encode_offer_plain(offer);
	const auto decoded = lan::decode_offer_plain(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK(decoded->folder_id.empty());
	LAN_CHECK_EQ(decoded->group_id, offer.group_id);
	LAN_CHECK_EQ(decoded->filename, std::string("src/main.cpp"));

	offer.folder_id = lan::random_hex_id();
	const auto both = lan::encode_offer_plain(offer);
	const auto both_decoded = lan::decode_offer_plain(both.data(), both.size());
	LAN_REQUIRE(both_decoded.has_value());
	LAN_CHECK_EQ(both_decoded->folder_id, offer.folder_id);
	LAN_CHECK_EQ(both_decoded->group_id, offer.group_id);

	// A 1:1 folder offer (16 trailing bytes) still decodes with empty group_id.
	lan::FileOfferPlain folder_only;
	folder_only.filename = "a.txt";
	folder_only.mime = "text/plain";
	folder_only.sha256 = offer.sha256;
	folder_only.folder_id = lan::random_hex_id();
	const auto legacy = lan::encode_offer_plain(folder_only);
	const auto legacy_decoded = lan::decode_offer_plain(legacy.data(), legacy.size());
	LAN_REQUIRE(legacy_decoded.has_value());
	LAN_CHECK_EQ(legacy_decoded->folder_id, folder_only.folder_id);
	LAN_CHECK(legacy_decoded->group_id.empty());
}

LAN_TEST(folder_offer_plain_keeps_group_id) {
	lan::FolderOfferPlain offer;
	offer.root = "Project";
	offer.file_count = 1;
	offer.empty_dirs = {"empty"};
	offer.group_id = lan::random_hex_id();
	const auto encoded = lan::encode_folder_offer_plain(offer);
	const auto decoded = lan::decode_folder_offer_plain(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->root, std::string("Project"));
	LAN_CHECK_EQ(decoded->group_id, offer.group_id);
	LAN_REQUIRE(decoded->empty_dirs.size() == 1);
}

LAN_TEST(transfer_state_roundtrip_keeps_group_id) {
	const auto dir = make_temp_dir();
	lan::TransferState state;
	state.outgoing = true;
	state.transfer_id = lan::random_hex_id();
	state.peer_id = lan::random_hex_id();
	state.folder_id = lan::random_hex_id();
	state.group_id = lan::random_hex_id();
	state.path = dir / "src" / "main.cpp";
	state.filename = "src/main.cpp";
	state.mime = "text/plain";
	state.size = 42;
	state.sha256 = lan::sha256_bytes("payload");
	state.chunk_count = 1;
	state.acked_count = 0;
	lan::save_transfer_state(dir, state);
	const auto loaded = lan::load_transfer_state(dir, state.transfer_id);
	LAN_REQUIRE(loaded.has_value());
	LAN_CHECK_EQ(loaded->group_id, state.group_id);
	LAN_CHECK_EQ(loaded->folder_id, state.folder_id);
	std::filesystem::remove_all(dir);
}

LAN_TEST(fast_chunk_math_covers_the_whole_file) {
	const auto step = lan::kFileChunkPlainFast;
	LAN_CHECK_EQ(lan::file_chunk_count(0, step), uint32_t(1));
	LAN_CHECK_EQ(lan::file_chunk_count(step, step), uint32_t(1));
	LAN_CHECK_EQ(lan::file_chunk_count(step + 1, step), uint32_t(2));
	LAN_CHECK_EQ(lan::file_chunk_plain_size(step + 1, 0, step), step);
	LAN_CHECK_EQ(lan::file_chunk_plain_size(step + 1, 1, step), uint32_t(1));
	LAN_CHECK_EQ(lan::bytes_for_acked_chunks(step * 2 + 10, 1, step), uint64_t(step));
	LAN_CHECK_EQ(lan::acked_count_from_part_size(step * 2 + 10, step + 9, step), uint32_t(1));
	LAN_CHECK_EQ(lan::effective_chunk_size(0), lan::kFileChunkPlainMax);
}

LAN_TEST(plain_file_offer_roundtrip_and_rejects_garbage) {
	lan::FileOfferPlain meta;
	meta.filename = "speed.bin";
	meta.mime = "application/octet-stream";
	meta.sha256 = lan::sha256_bytes("payload");
	const auto encoded = lan::encode_plain_file_offer(lan::kFileChunkPlainFast, meta);
	LAN_CHECK(lan::is_plaintext_payload(encoded.data(), encoded.size()));
	const auto decoded = lan::decode_plain_file_offer(encoded.data(), encoded.size());
	LAN_REQUIRE(decoded.has_value());
	LAN_CHECK_EQ(decoded->chunk_size, lan::kFileChunkPlainFast);
	LAN_CHECK_EQ(decoded->meta.filename, std::string("speed.bin"));
	LAN_CHECK(decoded->meta.sha256 == meta.sha256);
	LAN_CHECK(!lan::is_plaintext_payload(encoded.data() + 1, encoded.size() - 1));
	LAN_CHECK(!lan::decode_plain_file_offer(encoded.data(), 3).has_value());
	const auto wrapped = lan::wrap_plaintext_payload(
		reinterpret_cast<const uint8_t *>("abcd"), 4);
	LAN_REQUIRE(lan::is_plaintext_payload(wrapped.data(), wrapped.size()));
	const auto inner = lan::unwrap_plaintext_payload(wrapped.data(), wrapped.size());
	LAN_REQUIRE(inner.has_value());
	LAN_CHECK(std::string(inner->begin(), inner->end()) == "abcd");
}

LAN_TEST(transfer_state_v3_keeps_plain_chunk_size) {
	const auto dir = make_temp_dir();
	lan::TransferState state;
	state.outgoing = true;
	state.transfer_id = lan::random_hex_id();
	state.peer_id = lan::random_hex_id();
	state.path = dir / "speed.bin";
	state.filename = "speed.bin";
	state.size = lan::kFileChunkPlainFast + 3;
	state.sha256 = lan::sha256_bytes("payload");
	state.chunk_size = lan::kFileChunkPlainFast;
	state.chunk_count = lan::file_chunk_count(state.size, state.chunk_size);
	state.acked_count = 1;
	state.encrypted = false;
	lan::save_transfer_state(dir, state);
	const auto loaded = lan::load_transfer_state(dir, state.transfer_id);
	LAN_REQUIRE(loaded.has_value());
	LAN_CHECK_EQ(loaded->chunk_size, state.chunk_size);
	LAN_CHECK_EQ(loaded->chunk_count, state.chunk_count);
	LAN_CHECK(loaded->encrypted == false);
	auto offered = *loaded;
	LAN_CHECK(lan::transfer_can_resume(*loaded, offered));
	offered.encrypted = true;
	LAN_CHECK(!lan::transfer_can_resume(*loaded, offered));
	offered = *loaded;
	offered.chunk_size = lan::kFileChunkPlainMax;
	LAN_CHECK(!lan::transfer_can_resume(*loaded, offered));
	std::filesystem::remove_all(dir);
}

LAN_TEST(sha256_hasher_matches_bytes_and_file) {
	const std::string body(128 * 1024 + 17, 'H');
	lan::Sha256Hasher hasher;
	hasher.update(body.data(), body.size());
	LAN_CHECK(hasher.finish() == lan::sha256_bytes(body));
}

LAN_TEST(delete_recalled_payload_removes_file_and_folder_only) {
	const auto root = make_temp_dir();
	const auto file = root / "received.bin";
	write_file(file, "hello");
	const auto folder = root / "received-folder";
	std::filesystem::create_directories(folder / "sub");
	write_file(folder / "sub" / "a.txt", "a");
	const auto sibling = root / "unrelated.txt";
	write_file(sibling, "keep");

	LAN_CHECK(lan::delete_recalled_payload(file, false) == lan::RecalledDeleteStatus::Deleted);
	LAN_CHECK(!std::filesystem::exists(file));
	LAN_CHECK(lan::delete_recalled_payload(file, false) == lan::RecalledDeleteStatus::AlreadyGone);

	LAN_CHECK(lan::delete_recalled_payload(folder, true) == lan::RecalledDeleteStatus::Deleted);
	LAN_CHECK(!std::filesystem::exists(folder));
	LAN_CHECK(std::filesystem::exists(sibling));

	LAN_CHECK(lan::delete_recalled_payload({}, false) == lan::RecalledDeleteStatus::None);
	LAN_CHECK(lan::delete_recalled_payload(root, false) == lan::RecalledDeleteStatus::Refused);
	LAN_CHECK(std::filesystem::exists(root));
	LAN_CHECK(std::filesystem::exists(sibling));
	std::filesystem::remove_all(root);
}

LAN_TEST(delete_recalled_payload_refuses_root_and_directory_as_file) {
	LAN_CHECK(lan::delete_recalled_payload(std::filesystem::path("/"), true)
		== lan::RecalledDeleteStatus::Refused);
	const auto root = make_temp_dir();
	LAN_CHECK(lan::delete_recalled_payload(root, false)
		== lan::RecalledDeleteStatus::Refused);
	LAN_CHECK(std::filesystem::exists(root));
	std::filesystem::remove_all(root);
}
