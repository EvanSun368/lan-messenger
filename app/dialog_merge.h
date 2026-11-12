#pragma once

#include "models.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

struct PeerHint {
	QString id;
	QString name;
	QString ip;
};

// True when the list title is just the chat id (or its 8-char hex prefix).
bool is_placeholder_name(const QString &chatId, const QString &name);
bool is_usable_display_name(const QString &chatId, const QString &name);
bool names_equivalent(const QString &a, const QString &b);

// Prefer a real name from history, then a saved hint, then the hex prefix.
QString restored_display_name(
	const QString &chatId,
	const QString &selfId,
	const QVector<ChatMessage> &messages,
	const QString &savedName = {});

// Offline 1:1 leftover of a live peer: same id is already the same row;
// otherwise same last-known IP plus display name. If the leftover has no
// saved IP (legacy history), the display name may be used only when it is
// a real name and no other *online* peer shares it. Two online peers are
// never merged, even when they share a hostname.
bool is_offline_leftover_of_live(
	const Dialog &leftover,
	const Dialog &live,
	const QVector<Dialog> &dialogs);

QStringList leftover_ids_to_merge(
	const QVector<Dialog> &dialogs,
	const Dialog &live);

// Move source history into the canonical chat. Rewrites chatId and the
// leftover peer's fromId. Sorts by time and drops duplicate message ids.
QVector<ChatMessage> merge_chat_histories(
	const QString &canonicalId,
	const QVector<ChatMessage> &canonical,
	const QString &sourceId,
	const QVector<ChatMessage> &source);

struct DialogBook {
	QString selfId;
	QString activeChat;
	QVector<Dialog> dialogs;
	QHash<QString, QVector<ChatMessage>> messages;
};

void apply_peer_hints(DialogBook &book, const QVector<PeerHint> &hints);

// Merge matching offline leftovers into the live peer. Returns source ids.
// Combined history is stored under liveId; source rows are removed.
QStringList merge_offline_leftovers_into(DialogBook &book, const QString &liveId);
