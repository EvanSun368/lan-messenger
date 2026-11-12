#include "history_store.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString kindName(ChatMessage::Kind kind) {
	switch (kind) {
	case ChatMessage::Kind::File:
		return QStringLiteral("file");
	case ChatMessage::Kind::Folder:
		return QStringLiteral("folder");
	case ChatMessage::Kind::Service:
		return QStringLiteral("service");
	case ChatMessage::Kind::Sticker:
		return QStringLiteral("sticker");
	case ChatMessage::Kind::Deleted:
		return QStringLiteral("deleted");
	case ChatMessage::Kind::Text:
	default:
		return QStringLiteral("text");
	}
}

ChatMessage::Kind kindFromName(const QString &name) {
	if (name == QLatin1String("file")) {
		return ChatMessage::Kind::File;
	}
	if (name == QLatin1String("folder")) {
		return ChatMessage::Kind::Folder;
	}
	if (name == QLatin1String("service")) {
		return ChatMessage::Kind::Service;
	}
	if (name == QLatin1String("sticker")) {
		return ChatMessage::Kind::Sticker;
	}
	if (name == QLatin1String("deleted")) {
		return ChatMessage::Kind::Deleted;
	}
	return ChatMessage::Kind::Text;
}

QJsonObject toJson(const ChatMessage &msg) {
	QJsonObject obj;
	obj.insert(QStringLiteral("id"), msg.id);
	obj.insert(QStringLiteral("chatId"), msg.chatId);
	obj.insert(QStringLiteral("fromId"), msg.fromId);
	obj.insert(QStringLiteral("fromName"), msg.fromName);
	obj.insert(QStringLiteral("text"), msg.text);
	obj.insert(QStringLiteral("timestampMs"), QString::number(msg.timestampMs));
	obj.insert(QStringLiteral("outgoing"), msg.outgoing);
	obj.insert(QStringLiteral("acked"), msg.acked);
	obj.insert(QStringLiteral("kind"), kindName(msg.kind));
	obj.insert(QStringLiteral("transferId"), msg.transferId);
	obj.insert(QStringLiteral("filename"), msg.filename);
	obj.insert(QStringLiteral("mime"), msg.mime);
	obj.insert(QStringLiteral("size"), QString::number(msg.size));
	obj.insert(QStringLiteral("path"), msg.path);
	obj.insert(QStringLiteral("error"), msg.error);
	obj.insert(QStringLiteral("failed"), msg.failed);
	obj.insert(QStringLiteral("complete"), msg.complete);
	obj.insert(QStringLiteral("pendingAccept"), msg.pendingAccept);
	obj.insert(QStringLiteral("verified"), msg.verified);
	return obj;
}

ChatMessage fromJson(const QJsonObject &obj) {
	ChatMessage msg;
	msg.id = obj.value(QStringLiteral("id")).toString();
	msg.chatId = obj.value(QStringLiteral("chatId")).toString();
	msg.fromId = obj.value(QStringLiteral("fromId")).toString();
	msg.fromName = obj.value(QStringLiteral("fromName")).toString();
	msg.text = obj.value(QStringLiteral("text")).toString();
	msg.timestampMs = obj.value(QStringLiteral("timestampMs")).toString().toLongLong();
	msg.outgoing = obj.value(QStringLiteral("outgoing")).toBool();
	msg.acked = obj.value(QStringLiteral("acked")).toBool();
	msg.kind = kindFromName(obj.value(QStringLiteral("kind")).toString());
	msg.transferId = obj.value(QStringLiteral("transferId")).toString();
	msg.filename = obj.value(QStringLiteral("filename")).toString();
	msg.mime = obj.value(QStringLiteral("mime")).toString();
	msg.size = obj.value(QStringLiteral("size")).toString().toULongLong();
	msg.bytesDone = msg.size;
	msg.path = obj.value(QStringLiteral("path")).toString();
	msg.error = obj.value(QStringLiteral("error")).toString();
	msg.failed = obj.value(QStringLiteral("failed")).toBool();
	msg.complete = obj.value(QStringLiteral("complete")).toBool(true);
	msg.pendingAccept = obj.value(QStringLiteral("pendingAccept")).toBool(false);
	msg.verified = obj.value(QStringLiteral("verified")).toBool(false);
	return msg;
}

} // namespace

HistoryStore::HistoryStore(std::filesystem::path dir)
	: dir_(std::move(dir)) {
	std::error_code ec;
	std::filesystem::create_directories(dir_, ec);
}

QVector<ChatMessage> HistoryStore::load(const QString &chatId) const {
	QVector<ChatMessage> out;
	if (chatId.isEmpty()) {
		return out;
	}
	QFile file(QString::fromStdString((dir_ / (chatId.toStdString() + ".json")).string()));
	if (!file.open(QIODevice::ReadOnly)) {
		return out;
	}
	const auto doc = QJsonDocument::fromJson(file.readAll());
	if (!doc.isArray()) {
		return out;
	}
	for (const auto value : doc.array()) {
		out.push_back(fromJson(value.toObject()));
	}
	return out;
}

void HistoryStore::save(const QString &chatId, const QVector<ChatMessage> &messages) const {
	if (chatId.isEmpty()) {
		return;
	}
	std::error_code ec;
	std::filesystem::create_directories(dir_, ec);
	QJsonArray array;
	const int start = messages.size() > 500 ? messages.size() - 500 : 0;
	for (int i = start; i < messages.size(); ++i) {
		array.append(toJson(messages[i]));
	}
	QFile file(QString::fromStdString((dir_ / (chatId.toStdString() + ".json")).string()));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return;
	}
	file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}


QStringList HistoryStore::chatIds() const {
	QStringList out;
	std::error_code ec;
	if (!std::filesystem::exists(dir_, ec)) {
		return out;
	}
	for (const auto &entry : std::filesystem::directory_iterator(dir_, ec)) {
		if (ec || !entry.is_regular_file(ec)) {
			continue;
		}
		const auto name = QString::fromStdString(entry.path().filename().u8string());
		if (name.endsWith(QStringLiteral(".json")) && name.size() > 5
			&& name != QLatin1String("peers.json")) {
			out.push_back(name.left(name.size() - 5));
		}
	}
	out.sort();
	return out;
}

void HistoryStore::remove(const QString &chatId) const {
	if (chatId.isEmpty()) {
		return;
	}
	std::error_code ec;
	std::filesystem::remove(dir_ / (chatId.toStdString() + ".json"), ec);
}

QVector<PeerHint> HistoryStore::loadHints() const {
	QVector<PeerHint> out;
	QFile file(QString::fromStdString((dir_ / "peers.json").string()));
	if (!file.open(QIODevice::ReadOnly)) {
		return out;
	}
	const auto doc = QJsonDocument::fromJson(file.readAll());
	if (!doc.isArray()) {
		return out;
	}
	for (const auto value : doc.array()) {
		const auto obj = value.toObject();
		PeerHint hint;
		hint.id = obj.value(QStringLiteral("id")).toString();
		hint.name = obj.value(QStringLiteral("name")).toString();
		hint.ip = obj.value(QStringLiteral("ip")).toString();
		if (!hint.id.isEmpty()) {
			out.push_back(hint);
		}
	}
	return out;
}

void HistoryStore::saveHints(const QVector<PeerHint> &hints) const {
	std::error_code ec;
	std::filesystem::create_directories(dir_, ec);
	QJsonArray array;
	for (const auto &hint : hints) {
		if (hint.id.isEmpty()) {
			continue;
		}
		QJsonObject obj;
		obj.insert(QStringLiteral("id"), hint.id);
		obj.insert(QStringLiteral("name"), hint.name);
		obj.insert(QStringLiteral("ip"), hint.ip);
		array.append(obj);
	}
	QFile file(QString::fromStdString((dir_ / "peers.json").string()));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return;
	}
	file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
