#include "dialog_merge.h"

#include <algorithm>
#include <QSet>

bool names_equivalent(const QString &a, const QString &b) {
	return !a.trimmed().isEmpty()
		&& a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive) == 0;
}

bool is_placeholder_name(const QString &chatId, const QString &name) {
	const auto trimmed = name.trimmed();
	if (trimmed.isEmpty() || chatId.isEmpty()) {
		return true;
	}
	if (trimmed.compare(chatId, Qt::CaseInsensitive) == 0) {
		return true;
	}
	return trimmed.compare(chatId.left(8), Qt::CaseInsensitive) == 0;
}

bool is_usable_display_name(const QString &chatId, const QString &name) {
	return !is_placeholder_name(chatId, name);
}

QString restored_display_name(
		const QString &chatId,
		const QString &selfId,
		const QVector<ChatMessage> &messages,
		const QString &savedName) {
	const auto fromHistory = name_from_history(chatId, selfId, messages);
	if (is_usable_display_name(chatId, fromHistory)) {
		return fromHistory;
	}
	if (is_usable_display_name(chatId, savedName)) {
		return savedName;
	}
	return fromHistory;
}

bool is_offline_leftover_of_live(
		const Dialog &leftover,
		const Dialog &live,
		const QVector<Dialog> &dialogs) {
	if (leftover.isGroup || live.isGroup) {
		return false;
	}
	if (leftover.id.isEmpty() || live.id.isEmpty() || leftover.id == live.id) {
		return false;
	}
	if (leftover.online || !live.online) {
		return false;
	}
	if (!is_usable_display_name(leftover.id, leftover.name)
		|| !is_usable_display_name(live.id, live.name)
		|| !names_equivalent(leftover.name, live.name)) {
		return false;
	}
	if (!leftover.ip.isEmpty() && !live.ip.isEmpty()) {
		return leftover.ip == live.ip;
	}
	if (!leftover.ip.isEmpty()) {
		return false;
	}
	int onlineWithName = 0;
	for (const auto &item : dialogs) {
		if (item.isGroup || !item.online) {
			continue;
		}
		if (is_usable_display_name(item.id, item.name)
			&& names_equivalent(item.name, live.name)) {
			++onlineWithName;
		}
	}
	return onlineWithName == 1;
}

QStringList leftover_ids_to_merge(
		const QVector<Dialog> &dialogs,
		const Dialog &live) {
	QStringList ids;
	for (const auto &item : dialogs) {
		if (is_offline_leftover_of_live(item, live, dialogs)) {
			ids.push_back(item.id);
		}
	}
	return ids;
}

QVector<ChatMessage> merge_chat_histories(
		const QString &canonicalId,
		const QVector<ChatMessage> &canonical,
		const QString &sourceId,
		const QVector<ChatMessage> &source) {
	QVector<ChatMessage> out;
	QSet<QString> seen;
	auto take = [&](const QVector<ChatMessage> &list, const QString &rewriteFrom) {
		for (auto msg : list) {
			msg.chatId = canonicalId;
			if (!rewriteFrom.isEmpty() && msg.fromId == rewriteFrom) {
				msg.fromId = canonicalId;
			}
			if (!msg.id.isEmpty() && seen.contains(msg.id)) {
				continue;
			}
			if (!msg.id.isEmpty()) {
				seen.insert(msg.id);
			}
			out.push_back(msg);
		}
	};
	take(canonical, {});
	take(source, sourceId);
	std::stable_sort(out.begin(), out.end(), [](const ChatMessage &a, const ChatMessage &b) {
		if (a.timestampMs != b.timestampMs) {
			return a.timestampMs < b.timestampMs;
		}
		return a.id < b.id;
	});
	return out;
}

void apply_peer_hints(DialogBook &book, const QVector<PeerHint> &hints) {
	QHash<QString, PeerHint> byId;
	for (const auto &hint : hints) {
		if (!hint.id.isEmpty()) {
			byId.insert(hint.id, hint);
		}
	}
	for (auto &dialog : book.dialogs) {
		if (dialog.isGroup) {
			continue;
		}
		const auto hint = byId.value(dialog.id);
		const auto messages = book.messages.value(dialog.id);
		const auto name = restored_display_name(
			dialog.id,
			book.selfId,
			messages,
			!hint.name.isEmpty() ? hint.name : dialog.name);
		if (is_usable_display_name(dialog.id, name)) {
			dialog.name = name;
		}
		if (dialog.ip.isEmpty() && !hint.ip.isEmpty()) {
			dialog.ip = hint.ip;
		}
	}
}

QStringList merge_offline_leftovers_into(DialogBook &book, const QString &liveId) {
	Dialog live;
	for (const auto &item : book.dialogs) {
		if (item.id == liveId) {
			live = item;
			break;
		}
	}
	if (live.id.isEmpty() || !live.online || live.isGroup) {
		return {};
	}
	const auto sources = leftover_ids_to_merge(book.dialogs, live);
	for (const auto &src : sources) {
		book.messages[liveId] = merge_chat_histories(
			liveId,
			book.messages.value(liveId),
			src,
			book.messages.value(src));
		book.messages.remove(src);
		if (book.activeChat == src) {
			book.activeChat = liveId;
		}
	}
	if (!sources.isEmpty()) {
		book.dialogs.erase(
			std::remove_if(
				book.dialogs.begin(),
				book.dialogs.end(),
				[&](const Dialog &item) { return sources.contains(item.id); }),
			book.dialogs.end());
		for (auto &item : book.dialogs) {
			if (item.id != liveId) {
				continue;
			}
			const auto &list = book.messages.value(liveId);
			if (!list.isEmpty()) {
				item.lastTime = list.last().timestampMs;
			}
			break;
		}
	}
	return sources;
}
