#pragma once

#include "identity.h"
#include "protocol.h"
#include "e2e.h"
#include "group.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lan {

struct PeerInfo {
	std::string peer_id;
	std::string display_name;
	std::string ip;
	uint16_t tcp_port = 0;
	std::vector<uint8_t> public_key;
	std::string first_name;
	std::string last_name;
	std::string bio;
};

struct IncomingText {
	std::string from_peer_id;
	std::string from_name;
	std::string msg_id;
	uint64_t timestamp_ms = 0;
	std::string body;
	// Empty for 1:1 chat. Set when the text was fanned out to a group.
	std::string group_id;
};

struct FileEvent {
	enum class Kind { Offered, Started, Progress, Completed, Failed };
	Kind kind = Kind::Failed;
	bool outgoing = false;
	bool is_folder = false;
	std::string peer_id;
	std::string transfer_id;
	std::string folder_id;
	// Empty for a 1:1 transfer. Set when the file or folder was sent in a group.
	std::string group_id;
	std::string filename;
	std::string mime;
	uint64_t size = 0;
	uint64_t bytes_done = 0;
	uint32_t files_total = 0;
	uint32_t files_done = 0;
	std::filesystem::path path;
	std::string error;
	bool verified = false;
};

struct GroupEvent {
	enum class Kind { Upserted, Removed };
	Kind kind = Kind::Removed;
	Group group;
};

struct IncomingRecall {
	std::string from_peer_id;
	std::string msg_id;
	std::string group_id;
};

struct NodeConfig {
	Identity identity;
	// Empty disables multicast. Default is the LANLink discovery group.
	std::string multicast_group = "239.255.87.42";
	uint16_t discovery_port = 48710;
	// Interface used for bind and multicast. "0.0.0.0" listens on all.
	std::string bind_address = "0.0.0.0";
	uint16_t tcp_port = 0; // 0 = ephemeral
	uint16_t udp_port = 0; // 0 = use discovery_port, or ephemeral if 0 and no multicast?
	// Optional official tde2e key. If empty, the node generates an ephemeral key.
	std::shared_ptr<E2EKey> e2e_key;
	// When true, file and folder payloads are sealed with tde2e (fail-closed
	// if a key is missing). Default is off: a faster plaintext path on a
	// trusted LAN. Text stays end-to-end encrypted either way.
	bool encrypt_files = false;
	// Directory for completed incoming files. Created on demand.
	std::filesystem::path incoming_dir;
	// When true (default, used by tests and the CLI), an incoming offer
	// is accepted into incoming_dir immediately. The GUI sets this false
	// so the user can accept, decline, or pick a save path.
	bool auto_accept_incoming = true;
	// Persisted transfer snapshots and group rosters. If empty, the node
	// uses incoming_dir / ".state".
	std::filesystem::path state_dir;
	std::chrono::milliseconds announce_interval{1000};
	std::chrono::milliseconds peer_timeout{4000};
};

class Node {
public:
	explicit Node(NodeConfig config);
	~Node();

	Node(const Node &) = delete;
	Node &operator=(const Node &) = delete;

	void start();
	void stop();
	// Poll sockets and due timers, up to timeout.
	void step(std::chrono::milliseconds timeout);
	// True when a connection has queued write bytes, or a file transfer
	// is in flight (offer accepted, not held, not finished). Idle
	// connected peers, connecting sockets, and held offers are false.
	[[nodiscard]] bool needs_spin() const;

	[[nodiscard]] const Identity &identity() const;
	[[nodiscard]] const std::vector<uint8_t> &public_key() const;
	[[nodiscard]] uint16_t tcp_port() const;
	[[nodiscard]] uint16_t udp_port() const;
	[[nodiscard]] std::vector<PeerInfo> peers() const;

	// Send announces also to this unicast address (used by tests,
	// and as a fallback when multicast is filtered).
	void add_discovery_target(const std::string &ip, uint16_t port);

	// Returns the message id. Throws if the peer is not known.
	std::string send_text(const std::string &peer_id, std::string_view body);
	// Stream a file from disk. Returns the transfer id. Throws if the peer
	// is not known or the path cannot be read. The file is encrypted with
	// the official tde2e envelope; missing keys fail closed.
	std::string send_file(
		const std::string &peer_id,
		const std::filesystem::path &path,
		std::string_view display_name = {},
		std::string_view mime = {});
	// Walk a directory and send it as a folder tree. Returns the folder id.
	std::string send_folder(
		const std::string &peer_id,
		const std::filesystem::path &dir);

	// Fan-out of a file or folder to every group member except the sender.
	// Each recipient gets a sealed 1:1 offer (protocol v3 frames). Returns
	// a batch id for the sender's own records.
	std::string send_group_file(
		const std::string &group_id,
		const std::filesystem::path &path,
		std::string_view display_name = {},
		std::string_view mime = {});
	std::string send_group_folder(
		const std::string &group_id,
		const std::filesystem::path &dir);

	std::string create_group(
		std::string_view name,
		const std::vector<std::string> &member_ids = {});
	bool add_group_member(const std::string &group_id, const std::string &peer_id);
	bool remove_group_member(const std::string &group_id, const std::string &peer_id);
	// Fan-out of encrypt_message_for_one to each member. Returns the msg id.
	std::string send_group_text(const std::string &group_id, std::string_view body);
	[[nodiscard]] std::vector<Group> groups() const;

	// Retract a sent text or file. chat_id is a peer id (1:1) or group id.
	// sent_ms is the original send timestamp; the 48-hour window is enforced
	// here. Returns false if the window has closed or the id is invalid.
	bool recall_message(
		const std::string &chat_id,
		const std::string &msg_id,
		bool is_group,
		uint64_t sent_ms);
	// Abort an in-progress file or folder transfer. id may be a transfer
	// id, a folder id, or a group fan-out batch id. Abort is final and
	// does not resume. Returns true if anything was cancelled.
	// Accept a pending incoming file or folder. dest may be empty to use
	// incoming_dir (with a unique name). Returns false if the id is not
	// a pending offer.
	bool accept_transfer(
		const std::string &id,
		const std::filesystem::path &dest = {});
	// Decline a pending incoming file or folder. Notifies the sender
	// with FileAbort / FolderAbort and does not write the payload.
	bool reject_transfer(const std::string &id);
	void set_incoming_dir(const std::filesystem::path &dir);
	[[nodiscard]] std::filesystem::path incoming_dir() const;
	// Test helper: if set, the next FileDigest uses this value instead
	// of the real stream hash (to exercise checksum mismatch).
	void test_force_outgoing_digest(const std::array<uint8_t, 32> &digest);
	bool abort_transfer(const std::string &id);
	// Updates the in-memory display name and forces a discovery announce.
	// Does not write identity files; the caller persists them.
	bool set_display_name(std::string_view name);
	// Optional first/last/bio announced to peers with the display name.
	void set_profile(std::string_view first, std::string_view last, std::string_view bio);
	void set_encrypt_files(bool enabled);
	[[nodiscard]] bool encrypt_files() const;

	std::vector<PeerInfo> take_peer_events();
	std::vector<IncomingText> take_texts();
	std::vector<std::string> take_acks();
	std::vector<FileEvent> take_file_events();
	std::vector<GroupEvent> take_group_events();
	std::vector<IncomingRecall> take_recalls();

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace lan
