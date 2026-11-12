#include "check.h"
#include "history_store.h"
#include "node_controller.h"
#include "step_policy.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

namespace {

NodeController::NetworkOptions loopback_net() {
	NodeController::NetworkOptions net;
	net.bindAddress = "127.0.0.1";
	net.multicastGroup.clear();
	net.discoveryPort = 0;
	net.udpPort = 0;
	net.announceInterval = 50ms;
	net.peerTimeout = 1500ms;
	return net;
}

void pump_for(std::chrono::milliseconds duration) {
	const auto deadline = std::chrono::steady_clock::now() + duration;
	while (std::chrono::steady_clock::now() < deadline) {
		QCoreApplication::processEvents();
		std::this_thread::sleep_for(2ms);
	}
}

bool wait_for_online_peer(NodeController &node, const QString &peerId) {
	const auto deadline = std::chrono::steady_clock::now() + 4s;
	while (std::chrono::steady_clock::now() < deadline) {
		QCoreApplication::processEvents();
		for (const auto &dialog : node.dialogs()) {
			if (dialog.id == peerId && dialog.online) {
				return true;
			}
		}
		std::this_thread::sleep_for(5ms);
	}
	return false;
}

QString first_online_peer(const NodeController &node) {
	for (const auto &dialog : node.dialogs()) {
		if (dialog.online && !dialog.isGroup) {
			return dialog.id;
		}
	}
	return {};
}

} // namespace

LAN_TEST(controller_does_not_persist_or_rebuild_when_peers_are_stable) {
	QTemporaryDir tmp;
	const auto root = std::filesystem::path(tmp.path().toStdString());
	NodeController alice(root / "alice", "Alice", loopback_net());
	NodeController bob(root / "bob", "Bob", loopback_net());
	alice.addDiscoveryTarget(QStringLiteral("127.0.0.1"), bob.udpPort());
	bob.addDiscoveryTarget(QStringLiteral("127.0.0.1"), alice.udpPort());
	LAN_REQUIRE(wait_for_online_peer(alice, bob.selfId()));
	LAN_REQUIRE(wait_for_online_peer(bob, alice.selfId()));

	// Let the first hint write and leftover scan finish.
	pump_for(150ms);
	const int hints = alice.hintPersistCount();
	const int merges = alice.leftoverMergeAttempts();
	LAN_CHECK(hints >= 1);
	int dialogs = 0;
	int chats = 0;
	QObject::connect(&alice, &NodeController::dialogsChanged, [&]() { ++dialogs; });
	QObject::connect(&alice, &NodeController::chatChanged, [&](const QString &) { ++chats; });
	pump_for(400ms);
	LAN_CHECK_EQ(alice.hintPersistCount(), hints);
	LAN_CHECK_EQ(alice.leftoverMergeAttempts(), merges);
	LAN_CHECK_EQ(dialogs, 0);
	LAN_CHECK_EQ(chats, 0);
}

LAN_TEST(controller_merges_leftover_once_when_live_peer_becomes_mergeable) {
	QTemporaryDir tmp;
	const auto root = std::filesystem::path(tmp.path().toStdString());
	const auto leftoverId = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	HistoryStore store(root / "alice" / "history");
	ChatMessage old;
	old.id = QStringLiteral("m-old");
	old.chatId = leftoverId;
	old.fromId = leftoverId;
	old.fromName = QStringLiteral("Bob");
	old.text = QStringLiteral("old hello");
	old.timestampMs = 1000;
	store.save(leftoverId, {old});
	PeerHint hint;
	hint.id = leftoverId;
	hint.name = QStringLiteral("Bob");
	hint.ip = QStringLiteral("127.0.0.1");
	store.saveHints({hint});

	NodeController alice(root / "alice", "Alice", loopback_net());
	NodeController bob(root / "bob", "Bob", loopback_net());
	alice.addDiscoveryTarget(QStringLiteral("127.0.0.1"), bob.udpPort());
	bob.addDiscoveryTarget(QStringLiteral("127.0.0.1"), alice.udpPort());
	LAN_REQUIRE(wait_for_online_peer(alice, bob.selfId()));

	pump_for(200ms);
	bool leftoverGone = true;
	bool sawOld = false;
	for (const auto &dialog : alice.dialogs()) {
		if (dialog.id == leftoverId) {
			leftoverGone = false;
		}
	}
	for (const auto &msg : alice.messages(bob.selfId())) {
		if (msg.text == QStringLiteral("old hello")) {
			sawOld = true;
		}
	}
	LAN_CHECK(leftoverGone);
	LAN_CHECK(sawOld);
	LAN_CHECK(alice.leftoverMergeAttempts() >= 1);
	const int merges = alice.leftoverMergeAttempts();
	const int hints = alice.hintPersistCount();
	int dialogs = 0;
	QObject::connect(&alice, &NodeController::dialogsChanged, [&]() { ++dialogs; });
	pump_for(400ms);
	LAN_CHECK_EQ(alice.leftoverMergeAttempts(), merges);
	LAN_CHECK_EQ(alice.hintPersistCount(), hints);
	LAN_CHECK_EQ(dialogs, 0);
}

LAN_TEST(controller_throttles_transfer_progress_ui_and_returns_to_idle) {
	QTemporaryDir tmp;
	const auto root = std::filesystem::path(tmp.path().toStdString());
	NodeController alice(root / "alice", "Alice", loopback_net());
	NodeController bob(root / "bob", "Bob", loopback_net());
	alice.addDiscoveryTarget(QStringLiteral("127.0.0.1"), bob.udpPort());
	bob.addDiscoveryTarget(QStringLiteral("127.0.0.1"), alice.udpPort());
	LAN_REQUIRE(wait_for_online_peer(alice, bob.selfId()));
	LAN_REQUIRE(wait_for_online_peer(bob, alice.selfId()));
	pump_for(100ms);

	const auto source = root / "alice" / "downloads" / "bulk.bin";
	{
		std::string payload(8ull * 1024ull * 1024ull, 'U');
		std::ofstream out(source, std::ios::binary);
		out << payload;
	}

	int aliceChats = 0;
	int aliceDialogs = 0;
	QObject::connect(&alice, &NodeController::chatChanged, [&](const QString &) { ++aliceChats; });
	QObject::connect(&alice, &NodeController::dialogsChanged, [&]() { ++aliceDialogs; });

	alice.setActiveChat(bob.selfId());
	alice.sendPath(bob.selfId(), QString::fromStdString(source.u8string()));

	QString transferId;
	const auto offerDeadline = std::chrono::steady_clock::now() + 3s;
	while (std::chrono::steady_clock::now() < offerDeadline && transferId.isEmpty()) {
		QCoreApplication::processEvents();
		for (const auto &msg : bob.messages(alice.selfId())) {
			if (msg.kind == ChatMessage::Kind::File && msg.pendingAccept) {
				transferId = msg.transferId;
			}
		}
		std::this_thread::sleep_for(5ms);
	}
	LAN_REQUIRE(!transferId.isEmpty());
	LAN_REQUIRE(bob.acceptTransfer(transferId));

	bool bobComplete = false;
	bool aliceComplete = false;
	const auto deadline = std::chrono::steady_clock::now() + 10s;
	while (std::chrono::steady_clock::now() < deadline
		&& (!bobComplete || !aliceComplete)) {
		QCoreApplication::processEvents();
		for (const auto &msg : bob.messages(alice.selfId())) {
			if (msg.transferId == transferId && msg.complete) {
				bobComplete = true;
			}
		}
		for (const auto &msg : alice.messages(bob.selfId())) {
			if ((msg.transferId == transferId || msg.id == transferId)
				&& msg.complete) {
				aliceComplete = true;
			}
		}
		std::this_thread::sleep_for(2ms);
	}
	LAN_CHECK(bobComplete);
	LAN_CHECK(aliceComplete);
	// Let the completing tick's signals finish before the idle snapshot.
	pump_for(50ms);

	// Offered + Started + throttled Progress + Completed should stay
	// well below a per-chunk (or per-tick) signal storm.
	LAN_CHECK(aliceChats >= 2);
	LAN_CHECK(aliceChats <= 20);
	LAN_CHECK(aliceDialogs <= 20);

	const int chatsAfter = aliceChats;
	const int dialogsAfter = aliceDialogs;
	const int hints = alice.hintPersistCount();
	pump_for(300ms);
	LAN_CHECK_EQ(aliceChats, chatsAfter);
	LAN_CHECK_EQ(aliceDialogs, dialogsAfter);
	LAN_CHECK_EQ(alice.hintPersistCount(), hints);
	(void)first_online_peer(alice);
}
