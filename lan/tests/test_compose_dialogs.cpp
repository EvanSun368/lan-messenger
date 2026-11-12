#include "check.h"
#include "compose_bar.h"
#include "dialog_list.h"
#include "history_store.h"
#include "i18n.h"
#include "models.h"

#include <QDir>
#include <QString>
#include <QTemporaryDir>
#include <string>

namespace {

std::string s(const QString &text) {
	return text.toStdString();
}

} // namespace

LAN_TEST(compose_reserved_height_uses_grown_bar_not_min_fallback) {
	LAN_CHECK_EQ(reservedComposeHeight(false, 240, 52), 0);
	LAN_CHECK_EQ(reservedComposeHeight(true, 0, 52), 52);
	LAN_CHECK_EQ(reservedComposeHeight(true, 52, 52), 52);
	// Official max field is 224, plus 8 px padding on each side.
	LAN_CHECK_EQ(reservedComposeHeight(true, 240, 52), 240);
	LAN_CHECK_EQ(reservedComposeHeight(true, 72, 52), 72);
	// A missing / invalid sizeHint must not force the 52 px fallback
	// once the bar has already grown.
	LAN_CHECK(reservedComposeHeight(true, 240, 52) != 52);
}

LAN_TEST(dialog_preview_labels_offline_peers) {
	i18n::setLanguage(Language::ChineseSimplified);
	Dialog online;
	online.name = QStringLiteral("Bob");
	online.online = true;
	LAN_CHECK_EQ(s(dialogPreviewText(online)), std::string("在局域网中"));

	Dialog offlineEmpty;
	offlineEmpty.name = QStringLiteral("Bob");
	offlineEmpty.online = false;
	LAN_CHECK_EQ(s(dialogPreviewText(offlineEmpty)), std::string("不在线"));

	Dialog offlineHistory;
	offlineHistory.name = QStringLiteral("Bob");
	offlineHistory.online = false;
	offlineHistory.lastText = QStringLiteral("hello");
	LAN_CHECK_EQ(s(dialogPreviewText(offlineHistory)), std::string("不在线 · hello"));

	Dialog group;
	group.isGroup = true;
	group.online = false;
	group.lastText = QStringLiteral("hi");
	LAN_CHECK_EQ(s(dialogPreviewText(group)), std::string("hi"));

	i18n::setLanguage(Language::ChineseTraditional);
	LAN_CHECK_EQ(s(dialogPreviewText(offlineEmpty)), std::string("離線"));

	i18n::setLanguage(Language::English);
	LAN_CHECK_EQ(s(dialogPreviewText(offlineEmpty)), std::string("offline"));
	LAN_CHECK_EQ(s(dialogPreviewText(offlineHistory)), std::string("offline · hello"));
}

LAN_TEST(can_send_requires_an_online_peer) {
	Dialog bob;
	bob.id = QStringLiteral("bob");
	bob.online = true;
	LAN_CHECK(can_send_to_dialog(bob, QStringLiteral("self"), {bob}));

	bob.online = false;
	LAN_CHECK(!can_send_to_dialog(bob, QStringLiteral("self"), {bob}));

	Dialog group;
	group.id = QStringLiteral("g1");
	group.isGroup = true;
	group.memberIds = QStringList{QStringLiteral("self"), QStringLiteral("bob")};
	LAN_CHECK(!can_send_to_dialog(group, QStringLiteral("self"), {bob, group}));

	bob.online = true;
	LAN_CHECK(can_send_to_dialog(group, QStringLiteral("self"), {bob, group}));
}

LAN_TEST(history_store_lists_saved_chats_and_names) {
	QTemporaryDir tmp;
	LAN_REQUIRE(tmp.isValid());
	HistoryStore store(std::filesystem::path(tmp.path().toStdString()) / "history");

	ChatMessage incoming;
	incoming.id = QStringLiteral("m1");
	incoming.chatId = QStringLiteral("peer-bob");
	incoming.fromId = QStringLiteral("peer-bob");
	incoming.fromName = QStringLiteral("Bob");
	incoming.text = QStringLiteral("hello");
	incoming.timestampMs = 1;

	ChatMessage outgoing;
	outgoing.id = QStringLiteral("m2");
	outgoing.chatId = QStringLiteral("peer-bob");
	outgoing.fromId = QStringLiteral("self");
	outgoing.fromName = QStringLiteral("Me");
	outgoing.text = QStringLiteral("hi");
	outgoing.outgoing = true;
	outgoing.timestampMs = 2;

	store.save(QStringLiteral("peer-bob"), {incoming, outgoing});
	const auto ids = store.chatIds();
	LAN_REQUIRE(ids.size() == 1);
	LAN_CHECK_EQ(s(ids[0]), std::string("peer-bob"));

	const auto loaded = store.load(QStringLiteral("peer-bob"));
	LAN_CHECK_EQ(s(name_from_history(QStringLiteral("peer-bob"), QStringLiteral("self"), loaded)),
		std::string("Bob"));
	LAN_CHECK(!history_looks_like_group(QStringLiteral("peer-bob"), QStringLiteral("self"), loaded));

	ChatMessage fromCarol;
	fromCarol.id = QStringLiteral("g1");
	fromCarol.chatId = QStringLiteral("group-1");
	fromCarol.fromId = QStringLiteral("peer-carol");
	fromCarol.fromName = QStringLiteral("Carol");
	fromCarol.text = QStringLiteral("hey");
	LAN_CHECK(history_looks_like_group(
		QStringLiteral("group-1"), QStringLiteral("self"), {fromCarol}));
}
