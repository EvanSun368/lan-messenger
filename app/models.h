#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

struct ChatMessage {
	enum class Kind { Text, File, Folder, Service, Sticker, Deleted };

	QString id;
	QString chatId;
	QString fromId;
	QString fromName;
	QString text;
	qint64 timestampMs = 0;
	bool outgoing = false;
	bool acked = false;
	Kind kind = Kind::Text;
	QString transferId;
	QString filename;
	QString mime;
	quint64 size = 0;
	quint64 bytesDone = 0;
	QString path;
	QString error;
	bool failed = false;
	bool complete = false;
	bool pendingAccept = false;
	bool verified = false;
};

struct Dialog {
	QString id;
	bool isGroup = false;
	QString name;
	QString lastText;
	qint64 lastTime = 0;
	int unread = 0;
	bool online = false;
	QStringList memberIds;
	QString creatorId;
	QString ip;
	QString firstName;
	QString lastName;
	QString bio;
};

inline QString format_size(quint64 bytes) {
	if (bytes < 1024) {
		return QString::number(bytes) + QStringLiteral(" B");
	}
	if (bytes < 1024ull * 1024ull) {
		return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
	}
	if (bytes < 1024ull * 1024ull * 1024ull) {
		return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
	}
	return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

inline bool is_image_name(const QString &name, const QString &mime) {
	if (mime.startsWith(QStringLiteral("image/"))) {
		return true;
	}
	const auto lower = name.toLower();
	return lower.endsWith(QStringLiteral(".png"))
		|| lower.endsWith(QStringLiteral(".jpg"))
		|| lower.endsWith(QStringLiteral(".jpeg"))
		|| lower.endsWith(QStringLiteral(".gif"))
		|| lower.endsWith(QStringLiteral(".webp"))
		|| lower.endsWith(QStringLiteral(".bmp"));
}

inline bool can_recall_message(const ChatMessage &msg, qint64 nowMs) {
	if (!msg.outgoing || msg.kind == ChatMessage::Kind::Service
		|| msg.kind == ChatMessage::Kind::Deleted || msg.id.isEmpty()) {
		return false;
	}
	if (msg.timestampMs <= 0) {
		return false;
	}
	return nowMs >= msg.timestampMs && (nowMs - msg.timestampMs) <= (48ll * 60ll * 60ll * 1000ll);
}

inline bool transfer_in_progress(const ChatMessage &msg) {
	return (msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder)
		&& !msg.complete && !msg.failed;
}

// A 1:1 chat is sendable only while that peer is online. A group is
// sendable if at least one other member is currently online. There is
// no store-and-forward server.
inline bool can_send_to_dialog(
		const Dialog &dialog,
		const QString &selfId,
		const QVector<Dialog> &dialogs) {
	if (dialog.id.isEmpty()) {
		return false;
	}
	if (!dialog.isGroup) {
		return dialog.online;
	}
	for (const auto &memberId : dialog.memberIds) {
		if (memberId == selfId || memberId.isEmpty()) {
			continue;
		}
		for (const auto &peer : dialogs) {
			if (peer.id == memberId && peer.online) {
				return true;
			}
		}
	}
	return false;
}

inline QString name_from_history(
		const QString &chatId,
		const QString &selfId,
		const QVector<ChatMessage> &messages) {
	for (const auto &msg : messages) {
		if (!msg.fromName.isEmpty() && msg.fromId == chatId) {
			return msg.fromName;
		}
	}
	for (const auto &msg : messages) {
		if (!msg.fromName.isEmpty() && msg.fromId != selfId && !msg.fromId.isEmpty()) {
			return msg.fromName;
		}
	}
	return chatId.left(8);
}

inline bool history_looks_like_group(
		const QString &chatId,
		const QString &selfId,
		const QVector<ChatMessage> &messages) {
	for (const auto &msg : messages) {
		if (!msg.fromId.isEmpty() && msg.fromId != selfId && msg.fromId != chatId) {
			return true;
		}
	}
	return false;
}
