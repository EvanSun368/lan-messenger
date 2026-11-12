#include "check.h"
#include "dialog_merge.h"
#include "file_bubble.h"
#include "history_store.h"
#include "i18n.h"

#include <QTemporaryDir>
#include <string>

namespace {

std::string s(const QString &text) {
	return text.toStdString();
}

Dialog peer(const QString &id, const QString &name, bool online, const QString &ip = {}) {
	Dialog dialog;
	dialog.id = id;
	dialog.name = name;
	dialog.online = online;
	dialog.ip = ip;
	return dialog;
}

ChatMessage text_msg(
		const QString &id,
		const QString &chatId,
		const QString &fromId,
		const QString &fromName,
		const QString &text,
		qint64 time,
		bool outgoing = false) {
	ChatMessage msg;
	msg.id = id;
	msg.chatId = chatId;
	msg.fromId = fromId;
	msg.fromName = fromName;
	msg.text = text;
	msg.timestampMs = time;
	msg.outgoing = outgoing;
	return msg;
}

bool overlaps(const QRect &a, const QRect &b) {
	return !a.isEmpty() && !b.isEmpty() && a.intersects(b);
}

} // namespace

LAN_TEST(placeholder_name_is_hex_prefix_or_full_id) {
	const auto id = QStringLiteral("d5d2a4f2cafebabe0123456789abcdef");
	LAN_CHECK(is_placeholder_name(id, QStringLiteral("d5d2a4f2")));
	LAN_CHECK(is_placeholder_name(id, id));
	LAN_CHECK(is_placeholder_name(id, QString()));
	LAN_CHECK(!is_placeholder_name(id, QStringLiteral("TEST-DESKTOP")));
	LAN_CHECK(is_usable_display_name(id, QStringLiteral("TEST-DESKTOP")));
	LAN_CHECK(names_equivalent(QStringLiteral("TEST-DESKTOP"), QStringLiteral("test-desktop")));
}

LAN_TEST(restored_name_prefers_history_then_saved_hint) {
	const auto chat = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	const auto self = QStringLiteral("self");
	auto incoming = text_msg(
		QStringLiteral("m1"),
		chat,
		chat,
		QStringLiteral("TEST-DESKTOP"),
		QStringLiteral("hi"),
		1);
	LAN_CHECK_EQ(
		s(restored_display_name(chat, self, {incoming}, QStringLiteral("Other"))),
		std::string("TEST-DESKTOP"));

	auto outgoing = text_msg(
		QStringLiteral("m2"),
		chat,
		self,
		QStringLiteral("Me"),
		QStringLiteral("yo"),
		2,
		true);
	LAN_CHECK_EQ(
		s(restored_display_name(chat, self, {outgoing}, QStringLiteral("TEST-DESKTOP"))),
		std::string("TEST-DESKTOP"));
	LAN_CHECK_EQ(
		s(restored_display_name(chat, self, {outgoing}, QString())),
		std::string("aaaaaaaa"));
}

LAN_TEST(merge_offline_leftover_and_live_peer_keeps_history) {
	const auto liveId = QStringLiteral("cccccccccccccccccccccccccccccccc");
	const auto oldId = QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	const auto self = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	DialogBook book;
	book.selfId = self;

	Dialog live = peer(liveId, QStringLiteral("TEST-DESKTOP"), true, QStringLiteral("192.168.1.8"));
	Dialog leftover = peer(oldId, QStringLiteral("TEST-DESKTOP"), false, QStringLiteral("192.168.1.8"));
	leftover.lastTime = 10;
	book.dialogs = {leftover, live};
	book.messages[oldId] = {
		text_msg(QStringLiteral("o1"), oldId, oldId, QStringLiteral("TEST-DESKTOP"), QStringLiteral("old"), 10),
		text_msg(QStringLiteral("o2"), oldId, self, QStringLiteral("Me"), QStringLiteral("reply"), 20, true),
	};
	book.messages[liveId] = {
		text_msg(QStringLiteral("n1"), liveId, liveId, QStringLiteral("TEST-DESKTOP"), QStringLiteral("new"), 30),
	};
	book.activeChat = oldId;

	const auto merged = merge_offline_leftovers_into(book, liveId);
	LAN_REQUIRE(merged.size() == 1);
	LAN_CHECK_EQ(s(merged[0]), s(oldId));
	LAN_REQUIRE(book.dialogs.size() == 1);
	LAN_CHECK_EQ(s(book.dialogs[0].id), s(liveId));
	LAN_CHECK(book.dialogs[0].online);
	LAN_CHECK_EQ(s(book.dialogs[0].name), std::string("TEST-DESKTOP"));
	LAN_CHECK_EQ(s(book.activeChat), s(liveId));
	LAN_CHECK(!book.messages.contains(oldId));
	const auto &history = book.messages[liveId];
	LAN_REQUIRE(history.size() == 3);
	LAN_CHECK_EQ(s(history[0].text), std::string("old"));
	LAN_CHECK_EQ(s(history[0].chatId), s(liveId));
	LAN_CHECK_EQ(s(history[0].fromId), s(liveId));
	LAN_CHECK_EQ(s(history[1].text), std::string("reply"));
	LAN_CHECK_EQ(s(history[1].fromId), s(self));
	LAN_CHECK_EQ(s(history[2].text), std::string("new"));
}

LAN_TEST(legacy_nameless_ip_does_not_block_unique_name_merge) {
	const auto liveId = QStringLiteral("cccccccccccccccccccccccccccccccc");
	const auto oldId = QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	DialogBook book;
	book.dialogs = {
		peer(oldId, QStringLiteral("TEST-DESKTOP"), false), // no saved IP
		peer(liveId, QStringLiteral("TEST-DESKTOP"), true, QStringLiteral("10.0.0.2")),
	};
	book.messages[oldId] = {
		text_msg(QStringLiteral("o1"), oldId, oldId, QStringLiteral("TEST-DESKTOP"), QStringLiteral("hi"), 1),
	};
	const auto merged = merge_offline_leftovers_into(book, liveId);
	LAN_CHECK(merged.size() == 1);
	LAN_CHECK(book.dialogs.size() == 1);
}

LAN_TEST(hex_placeholder_leftover_is_not_merged) {
	const auto liveId = QStringLiteral("cccccccccccccccccccccccccccccccc");
	const auto oldId = QStringLiteral("d5d2a4f2cafebabe0123456789abcdef");
	Dialog live = peer(liveId, QStringLiteral("TEST-DESKTOP"), true, QStringLiteral("10.0.0.2"));
	Dialog leftover = peer(oldId, QStringLiteral("d5d2a4f2"), false);
	LAN_CHECK(!is_offline_leftover_of_live(leftover, live, {leftover, live}));
}

LAN_TEST(two_live_peers_with_same_name_are_not_merged) {
	const auto a = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	const auto b = QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	Dialog first = peer(a, QStringLiteral("TEST-DESKTOP"), true, QStringLiteral("10.0.0.2"));
	Dialog second = peer(b, QStringLiteral("TEST-DESKTOP"), true, QStringLiteral("10.0.0.2"));
	DialogBook book;
	book.dialogs = {first, second};
	book.messages[a] = {text_msg(QStringLiteral("m1"), a, a, QStringLiteral("TEST-DESKTOP"), QStringLiteral("x"), 1)};
	book.messages[b] = {text_msg(QStringLiteral("m2"), b, b, QStringLiteral("TEST-DESKTOP"), QStringLiteral("y"), 2)};
	LAN_CHECK(merge_offline_leftovers_into(book, a).isEmpty());
	LAN_CHECK(merge_offline_leftovers_into(book, b).isEmpty());
	LAN_CHECK(book.dialogs.size() == 2);
	LAN_CHECK(book.messages[a].size() == 1);
	LAN_CHECK(book.messages[b].size() == 1);
}

LAN_TEST(same_name_different_ip_offline_leftover_is_not_merged) {
	Dialog live = peer(
		QStringLiteral("cccccccccccccccccccccccccccccccc"),
		QStringLiteral("Bob"),
		true,
		QStringLiteral("10.0.0.2"));
	Dialog leftover = peer(
		QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
		QStringLiteral("Bob"),
		false,
		QStringLiteral("10.0.0.9"));
	LAN_CHECK(!is_offline_leftover_of_live(leftover, live, {leftover, live}));
}

LAN_TEST(apply_peer_hints_restores_name_and_ip) {
	const auto id = QStringLiteral("d5d2a4f2cafebabe0123456789abcdef");
	DialogBook book;
	book.selfId = QStringLiteral("self");
	book.dialogs = {peer(id, QStringLiteral("d5d2a4f2"), false)};
	book.messages[id] = {
		text_msg(QStringLiteral("m1"), id, QStringLiteral("self"), QStringLiteral("Me"), QStringLiteral("hi"), 1, true),
	};
	PeerHint hint;
	hint.id = id;
	hint.name = QStringLiteral("TEST-DESKTOP");
	hint.ip = QStringLiteral("192.168.1.8");
	apply_peer_hints(book, {hint});
	LAN_CHECK_EQ(s(book.dialogs[0].name), std::string("TEST-DESKTOP"));
	LAN_CHECK_EQ(s(book.dialogs[0].ip), std::string("192.168.1.8"));
}

LAN_TEST(history_store_removes_merged_chat_and_persists_hints) {
	QTemporaryDir tmp;
	LAN_REQUIRE(tmp.isValid());
	HistoryStore store(std::filesystem::path(tmp.path().toStdString()) / "history");
	auto msg = text_msg(
		QStringLiteral("m1"),
		QStringLiteral("old"),
		QStringLiteral("old"),
		QStringLiteral("Bob"),
		QStringLiteral("hi"),
		1);
	store.save(QStringLiteral("old"), {msg});
	store.save(QStringLiteral("live"), {msg});
	LAN_CHECK(store.chatIds().size() == 2);
	store.remove(QStringLiteral("old"));
	const auto ids = store.chatIds();
	LAN_REQUIRE(ids.size() == 1);
	LAN_CHECK_EQ(s(ids[0]), std::string("live"));

	PeerHint hint;
	hint.id = QStringLiteral("live");
	hint.name = QStringLiteral("Bob");
	hint.ip = QStringLiteral("10.0.0.2");
	store.saveHints({hint});
	const auto loaded = store.loadHints();
	LAN_REQUIRE(loaded.size() == 1);
	LAN_CHECK_EQ(s(loaded[0].name), std::string("Bob"));
	LAN_CHECK_EQ(s(loaded[0].ip), std::string("10.0.0.2"));
}

LAN_TEST(file_transfer_status_waiting_progress_and_rejected) {
	i18n::setLanguage(Language::ChineseSimplified);
	ChatMessage waiting;
	waiting.kind = ChatMessage::Kind::File;
	waiting.outgoing = true;
	waiting.pendingAccept = true;
	waiting.size = 167u * 1024u * 1024u;
	waiting.bytesDone = 0;
	LAN_CHECK_EQ(s(file_transfer_status(waiting)), std::string("等待接收"));
	LAN_CHECK(s(file_transfer_status(waiting)).find("0 B") == std::string::npos);

	ChatMessage progress = waiting;
	progress.pendingAccept = false;
	progress.bytesDone = 10u * 1024u * 1024u;
	LAN_CHECK_EQ(s(file_transfer_status(progress)), std::string("10.0 MB / 167.0 MB"));

	ChatMessage declined = waiting;
	declined.pendingAccept = false;
	declined.failed = true;
	declined.error = QStringLiteral("rejected");
	LAN_CHECK_EQ(s(file_transfer_status(declined)), std::string("拒绝接收"));

	i18n::setLanguage(Language::ChineseTraditional);
	LAN_CHECK_EQ(s(file_transfer_status(waiting)), std::string("等待接收"));
	LAN_CHECK_EQ(s(file_transfer_status(declined)), std::string("拒絕接收"));

	i18n::setLanguage(Language::English);
	LAN_CHECK_EQ(s(file_transfer_status(waiting)), std::string("Waiting to accept"));
	LAN_CHECK_EQ(s(file_transfer_status(declined)), std::string("Declined to receive"));
}

LAN_TEST(file_offer_layout_does_not_overlap_on_wide_or_narrow_bubbles) {
	auto check = [](const FileBubbleMetrics &metrics) {
		const auto layout = layout_file_bubble(metrics);
		LAN_CHECK(layout.width > 0);
		LAN_CHECK(layout.height > 0);
		LAN_CHECK(layout.icon.bottom() <= layout.height);
		LAN_CHECK(layout.name.right() <= layout.width);
		LAN_CHECK(!overlaps(layout.icon, layout.name));
		LAN_CHECK(!overlaps(layout.icon, layout.accept));
		LAN_CHECK(!overlaps(layout.name, layout.accept));
		LAN_CHECK(!overlaps(layout.name, layout.decline));
		LAN_CHECK(!overlaps(layout.status, layout.accept));
		LAN_CHECK(!overlaps(layout.accept, layout.decline));
		LAN_CHECK(!overlaps(layout.accept, layout.saveAs));
		LAN_CHECK(!overlaps(layout.decline, layout.saveAs));
		LAN_CHECK(!overlaps(layout.time, layout.accept));
		LAN_CHECK(!overlaps(layout.time, layout.decline));
		LAN_CHECK(!overlaps(layout.time, layout.saveAs));
		LAN_CHECK(!overlaps(layout.time, layout.name));
		LAN_CHECK(!overlaps(layout.time, layout.status));
		LAN_CHECK(layout.accept.right() <= layout.width);
		LAN_CHECK(layout.decline.right() <= layout.width);
		LAN_CHECK(layout.saveAs.right() <= layout.width);
		LAN_CHECK(layout.time.right() <= layout.width);
		LAN_CHECK(layout.time.bottom() <= layout.height);
		LAN_CHECK(layout.accept.top() >= layout.icon.bottom());
	};

	FileBubbleMetrics wide;
	wide.maxInnerWidth = 360;
	wide.incomingOffer = true;
	wide.acceptWidth = 48;
	wide.declineWidth = 48;
	wide.saveAsWidth = 84;
	wide.timeWidth = 36;
	check(wide);
	const auto wideLayout = layout_file_bubble(wide);
	LAN_CHECK(wideLayout.accept.y() == wideLayout.decline.y());

	FileBubbleMetrics zh;
	zh.maxInnerWidth = 160;
	zh.incomingOffer = true;
	zh.acceptWidth = 48;
	zh.declineWidth = 48;
	zh.saveAsWidth = 108;
	zh.timeWidth = 36;
	check(zh);
	const auto zhLayout = layout_file_bubble(zh);
	LAN_CHECK(zhLayout.saveAs.y() > zhLayout.decline.y());

	FileBubbleMetrics en;
	en.maxInnerWidth = 180;
	en.incomingOffer = true;
	en.acceptWidth = 72;
	en.declineWidth = 72;
	en.saveAsWidth = 160;
	en.timeWidth = 44;
	check(en);
}
