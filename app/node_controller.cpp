#include "node_controller.h"
#include <chrono>

#include "history_store.h"
#include "dialog_merge.h"
#include "step_policy.h"
#include "avatar.h"
#include "emoji_catalog.h"
#include "working_dir.h"
#include "i18n.h"
#include "transfer.h"

#include "e2e.h"
#include "identity.h"
#include "node.h"
#include "protocol.h"

#include <QDateTime>
#include <QImage>
#include <QFile>
#include <QIODevice>
#include <QFileInfo>
#include <QSet>
#include <QTimer>
#include <algorithm>
#include <stdexcept>

namespace {

QString fromStd(const std::string &text) {
	return QString::fromStdString(text);
}

std::string toStd(const QString &text) {
	return text.toStdString();
}

QString previewOf(const ChatMessage &msg) {
	if (msg.kind == ChatMessage::Kind::File) {
		return msg.filename.isEmpty() ? i18n::tr("file") : msg.filename;
	}
	if (msg.kind == ChatMessage::Kind::Folder) {
		return msg.filename.isEmpty() ? i18n::tr("folder") : msg.filename;
	}
	if (msg.kind == ChatMessage::Kind::Service) {
		return i18n::localize(msg.text);
	}
	if (msg.kind == ChatMessage::Kind::Sticker) {
		return i18n::tr("sticker");
	}
	if (msg.kind == ChatMessage::Kind::Deleted) {
		if (msg.text.isEmpty()) {
			return i18n::tr("message_deleted");
		}
		return i18n::localize(msg.text);
	}
	if (msg.text.size() > 100) {
		return msg.text.left(100) + QStringLiteral("…");
	}
	return msg.text;
}

QString mimeFor(const QString &path) {
	const auto lower = path.toLower();
	if (lower.endsWith(QLatin1String(".png"))) {
		return QStringLiteral("image/png");
	}
	if (lower.endsWith(QLatin1String(".jpg")) || lower.endsWith(QLatin1String(".jpeg"))) {
		return QStringLiteral("image/jpeg");
	}
	if (lower.endsWith(QLatin1String(".gif"))) {
		return QStringLiteral("image/gif");
	}
	if (lower.endsWith(QLatin1String(".webp"))) {
		return QStringLiteral("image/webp");
	}
	if (lower.endsWith(QLatin1String(".pdf"))) {
		return QStringLiteral("application/pdf");
	}
	if (lower.endsWith(QLatin1String(".txt")) || lower.endsWith(QLatin1String(".md"))) {
		return QStringLiteral("text/plain");
	}
	return QStringLiteral("application/octet-stream");
}

} // namespace

NodeController::NodeController(
		std::filesystem::path workingDir,
		std::string displayName,
		QObject *parent)
	: NodeController(std::move(workingDir), std::move(displayName), NetworkOptions{}, parent) {
}

NodeController::NodeController(
		std::filesystem::path workingDir,
		std::string displayName,
		NetworkOptions network,
		QObject *parent)
	: QObject(parent)
	, workingDir_(std::move(workingDir))
	, downloadsDir_(workingDir_ / "downloads") {
	std::filesystem::create_directories(workingDir_);
	std::filesystem::create_directories(downloadsDir_);
	const auto identityDir = workingDir_ / "identity";
	std::filesystem::create_directories(identityDir);

	const auto fingerprint = lan::current_machine_fingerprint(identityDir);
	const auto hostname = lan::current_hostname();
	auto name = lan::default_display_name_from(hostname, "lanlink");
	if (!displayName.empty()) {
		const auto normalized = lan::normalize_display_name(displayName);
		if (!normalized.empty()) {
			name = normalized;
		}
	}
	const auto identity = lan::load_or_create_identity(identityDir, name, fingerprint);
	auto key = std::make_shared<lan::E2EKey>(lan::load_or_create_e2e_key(identityDir));

	selfId_ = fromStd(identity.peer_id);
	selfName_ = fromStd(identity.display_name);
	selfFirst_ = readProfileField("first_name");
	selfLast_ = readProfileField("last_name");
	selfBio_ = readProfileField("bio");
	if (selfFirst_.isEmpty() && selfLast_.isEmpty() && !selfName_.isEmpty()) {
		const auto parts = selfName_.split(QChar(' '), Qt::SkipEmptyParts);
		if (!parts.isEmpty()) {
			selfFirst_ = parts.first();
			if (parts.size() > 1) {
				selfLast_ = QStringList(parts.mid(1)).join(QChar(' '));
			}
		}
	}
	const auto avatarPath = workingDir_ / "identity" / "avatar.png";
	if (std::filesystem::exists(avatarPath)) {
		setCachedAvatar(selfId_, QString::fromStdString(avatarPath.u8string()));
	}

	history_ = std::make_unique<HistoryStore>(workingDir_ / "history");

	lan::NodeConfig config;
	config.identity = identity;
	config.e2e_key = std::move(key);
	config.incoming_dir = downloadsDir_;
	config.state_dir = workingDir_ / "state";
	config.auto_accept_incoming = false;
	config.bind_address = network.bindAddress;
	config.multicast_group = network.multicastGroup;
	config.discovery_port = network.discoveryPort;
	config.udp_port = network.udpPort;
	config.tcp_port = network.tcpPort;
	config.announce_interval = network.announceInterval;
	config.peer_timeout = network.peerTimeout;
	node_ = std::make_unique<lan::Node>(std::move(config));
	applyProfileToNode();
	node_->start();

	QSet<QString> loadedIds;
	for (const auto &group : node_->groups()) {
		Dialog dialog;
		dialog.id = fromStd(group.group_id);
		dialog.isGroup = true;
		dialog.name = fromStd(group.name);
		dialog.creatorId = fromStd(group.creator_id);
		for (const auto &member : group.member_ids) {
			dialog.memberIds.push_back(fromStd(member));
		}
		messages_[dialog.id] = history_->load(dialog.id);
		if (!messages_[dialog.id].isEmpty()) {
			const auto &last = messages_[dialog.id].last();
			dialog.lastText = previewOf(last);
			dialog.lastTime = last.timestampMs;
		}
		dialogs_.push_back(dialog);
		loadedIds.insert(dialog.id);
	}
	// Keep 1:1 (and any leftover group) chats that have local history even
	// if the peer is not currently discovered on the LAN.
	for (const auto &chatId : history_->chatIds()) {
		if (chatId.isEmpty() || chatId == selfId_ || loadedIds.contains(chatId)) {
			continue;
		}
		const auto messages = history_->load(chatId);
		if (messages.isEmpty()) {
			continue;
		}
		messages_[chatId] = messages;
		Dialog dialog;
		dialog.id = chatId;
		dialog.isGroup = history_looks_like_group(chatId, selfId_, messages);
		dialog.online = false;
		dialog.name = name_from_history(chatId, selfId_, messages);
		const auto &last = messages.last();
		dialog.lastText = previewOf(last);
		dialog.lastTime = last.timestampMs;
		dialogs_.push_back(dialog);
		loadedIds.insert(chatId);
	}
	if (history_) {
		DialogBook book;
		book.selfId = selfId_;
		book.dialogs = dialogs_;
		book.messages = messages_;
		lastSavedHints_ = history_->loadHints();
		apply_peer_hints(book, lastSavedHints_);
		dialogs_ = book.dialogs;
	}

	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout, this, &NodeController::step);
	timer_->start(kIdleStepTimerMs);
	QTimer::singleShot(0, this, [this]() { emit selfReady(selfName_); });
}

NodeController::~NodeController() {
	if (node_) {
		node_->stop();
	}
}

QString NodeController::selfId() const {
	return selfId_;
}

QString NodeController::selfName() const {
	return selfName_;
}

std::filesystem::path NodeController::workingDir() const {
	return workingDir_;
}

std::filesystem::path NodeController::downloadsDir() const {
	return downloadsDir_;
}

quint16 NodeController::udpPort() const {
	return node_ ? quint16(node_->udp_port()) : 0;
}

void NodeController::addDiscoveryTarget(const QString &ip, quint16 port) {
	if (node_ && port != 0) {
		node_->add_discovery_target(toStd(ip), port);
	}
}

QVector<Dialog> NodeController::dialogs() const {
	auto out = dialogs_;
	std::sort(out.begin(), out.end(), [](const Dialog &a, const Dialog &b) {
		if (a.lastTime != b.lastTime) {
			return a.lastTime > b.lastTime;
		}
		return a.name.localeAwareCompare(b.name) < 0;
	});
	return out;
}

QVector<ChatMessage> NodeController::messages(const QString &chatId) const {
	return messages_.value(chatId);
}

Dialog NodeController::dialog(const QString &chatId) const {
	for (const auto &item : dialogs_) {
		if (item.id == chatId) {
			return item;
		}
	}
	return {};
}

QVector<Dialog> NodeController::onlinePeers() const {
	QVector<Dialog> out;
	for (const auto &item : dialogs_) {
		if (!item.isGroup && item.online) {
			out.push_back(item);
		}
	}
	return out;
}

void NodeController::setActiveChat(const QString &chatId) {
	activeChat_ = chatId;
	for (auto &item : dialogs_) {
		if (item.id == chatId) {
			item.unread = 0;
		}
	}
	emit dialogsChanged();
	emit chatChanged(chatId);
}

bool NodeController::sendText(const QString &chatId, const QString &text) {
	if (!node_ || chatId.isEmpty() || text.trimmed().isEmpty()) {
		return false;
	}
	const auto dialog = this->dialog(chatId);
	if (!can_send_to_dialog(dialog, selfId_, dialogs_)) {
		appendOfflineNotice(chatId);
		return false;
	}
	const std::string utf8 = toStd(text);
	size_t off = 0;
	bool sent = false;
	while (off < utf8.size()) {
		size_t n = std::min<size_t>(lan::kMaxTextPlain, utf8.size() - off);
		while (n > 0 && off + n < utf8.size()
			&& (static_cast<unsigned char>(utf8[off + n]) & 0xC0) == 0x80) {
			--n;
		}
		if (n == 0) {
			n = 1;
		}
		const QString piece = QString::fromUtf8(utf8.data() + off, int(n));
		ChatMessage msg;
		msg.chatId = chatId;
		msg.fromId = selfId_;
		msg.fromName = selfName_;
		msg.text = piece;
		msg.timestampMs = nowMs();
		msg.outgoing = true;
		msg.kind = ChatMessage::Kind::Text;
		try {
			if (dialog.isGroup) {
				msg.id = fromStd(node_->send_group_text(toStd(chatId), toStd(piece)));
			} else {
				msg.id = fromStd(node_->send_text(toStd(chatId), toStd(piece)));
			}
		} catch (const std::exception &) {
			// A later chunk failed; earlier pieces already went out.
			// Do not swallow the rest of a large paste by truncating it.
			break;
		}
		if (emoji::isStickerPayload(msg.text)) {
			msg.kind = ChatMessage::Kind::Sticker;
		}
		appendMessage(msg, false);
		sent = true;
		off += n;
	}
	return sent;
}

void NodeController::appendOfflineNotice(const QString &chatId) {
	if (chatId.isEmpty()) {
		return;
	}
	auto &list = messages_[chatId];
	if (!list.isEmpty()
		&& list.last().kind == ChatMessage::Kind::Service
		&& list.last().text == QStringLiteral("cannot_send_offline")) {
		emit chatChanged(chatId);
		return;
	}
	ChatMessage service;
	service.id = QStringLiteral("offline-") + QString::number(nowMs());
	service.chatId = chatId;
	service.fromId = selfId_;
	service.fromName = selfName_;
	service.text = QStringLiteral("cannot_send_offline");
	service.timestampMs = nowMs();
	service.kind = ChatMessage::Kind::Service;
	appendMessage(service, false);
}

void NodeController::sendPath(const QString &chatId, const QString &path) {
	if (!node_ || chatId.isEmpty() || path.isEmpty()) {
		return;
	}
	const auto dialog = this->dialog(chatId);
	if (!can_send_to_dialog(dialog, selfId_, dialogs_)) {
		appendOfflineNotice(chatId);
		return;
	}
	const QFileInfo info(path);
	const bool isDir = info.isDir();
	ChatMessage msg;
	msg.chatId = chatId;
	msg.fromId = selfId_;
	msg.fromName = selfName_;
	msg.timestampMs = nowMs();
	msg.outgoing = true;
	msg.kind = isDir ? ChatMessage::Kind::Folder : ChatMessage::Kind::File;
	msg.filename = info.fileName();
	msg.path = path;
	msg.mime = isDir ? QString() : mimeFor(path);
	if (!isDir) {
		msg.size = quint64(info.size());
	}
	try {
		if (dialog.isGroup) {
			if (isDir) {
				msg.id = fromStd(node_->send_group_folder(toStd(chatId), toStd(path)));
			} else {
				msg.id = fromStd(node_->send_group_file(
					toStd(chatId),
					toStd(path),
					toStd(msg.filename),
					toStd(msg.mime)));
			}
		} else if (isDir) {
			msg.id = fromStd(node_->send_folder(toStd(chatId), toStd(path)));
		} else {
			msg.id = fromStd(node_->send_file(
				toStd(chatId),
				toStd(path),
				toStd(msg.filename),
				toStd(msg.mime)));
		}
		msg.transferId = msg.id;
		msg.pendingAccept = true;
		msg.bytesDone = 0;
	} catch (const std::exception &) {
		return;
	}
	appendMessage(msg, false);
}

QString NodeController::createGroup(const QString &name, const QStringList &memberIds) {
	if (!node_ || name.trimmed().isEmpty()) {
		return {};
	}
	std::vector<std::string> members;
	for (const auto &id : memberIds) {
		members.push_back(toStd(id));
	}
	try {
		const auto groupId = fromStd(node_->create_group(toStd(name.trimmed()), members));
		Dialog dialog;
		dialog.id = groupId;
		dialog.isGroup = true;
		dialog.name = name.trimmed();
		dialog.memberIds = memberIds;
		dialog.memberIds.push_back(selfId_);
		dialog.creatorId = selfId_;
		dialog.lastTime = nowMs();
		upsertGroup(dialog);
		ChatMessage service;
		service.id = groupId + QStringLiteral("-created");
		service.chatId = groupId;
		service.fromId = selfId_;
		service.fromName = selfName_;
		service.text = i18n::tr("group_created");
		service.timestampMs = nowMs();
		service.outgoing = true;
		service.kind = ChatMessage::Kind::Service;
		appendMessage(service, false);
		return groupId;
	} catch (const std::exception &) {
		return {};
	}
}

bool NodeController::addGroupMember(const QString &groupId, const QString &peerId) {
	if (!node_) {
		return false;
	}
	try {
		return node_->add_group_member(toStd(groupId), toStd(peerId));
	} catch (const std::exception &) {
		return false;
	}
}

bool NodeController::removeGroupMember(const QString &groupId, const QString &peerId) {
	if (!node_) {
		return false;
	}
	try {
		return node_->remove_group_member(toStd(groupId), toStd(peerId));
	} catch (const std::exception &) {
		return false;
	}
}

void NodeController::step() {
	if (!node_) {
		return;
	}
	// LocalSend keeps the HTTP body streaming on the event loop. The
	// old 40 ms / poll(0) cadence only flushed a small socket buffer
	// each tick (~a few MiB/s on Windows). Drain briefly while bytes
	// are moving, then yield on a short non-zero interval so the UI
	// and the LAN are not starved. Idle ticks stay at 40 ms.
	const auto started = std::chrono::steady_clock::now();
	do {
		node_->step(std::chrono::milliseconds(0));
	} while (node_->needs_spin()
		&& std::chrono::steady_clock::now() - started
			< std::chrono::milliseconds(kSpinDrainMs));
	if (timer_) {
		timer_->setInterval(step_timer_interval_ms(node_->needs_spin()));
	}

	bool dialogsDirty = false;
	QSet<QString> chatsDirty;

	// Mark peers unseen this tick as possibly still online until timeout
	// is reflected by the node dropping them.
	QSet<QString> live;
	for (const auto &peer : node_->peers()) {
		live.insert(fromStd(peer.peer_id));
		const auto change = upsertPeer(
			fromStd(peer.peer_id),
			fromStd(peer.display_name),
			true,
			fromStd(peer.ip),
			fromStd(peer.first_name),
			fromStd(peer.last_name),
			fromStd(peer.bio));
		if (change.dialogChanged) {
			dialogsDirty = true;
		}
	}
	for (auto &item : dialogs_) {
		if (!item.isGroup) {
			const bool was = item.online;
			item.online = live.contains(item.id);
			if (was != item.online) {
				dialogsDirty = true;
			}
		}
	}
	for (const auto &peer : node_->take_peer_events()) {
		const auto change = upsertPeer(
			fromStd(peer.peer_id),
			fromStd(peer.display_name),
			true,
			fromStd(peer.ip),
			fromStd(peer.first_name),
			fromStd(peer.last_name),
			fromStd(peer.bio));
		if (change.dialogChanged) {
			dialogsDirty = true;
		}
	}

	for (const auto &text : node_->take_texts()) {
		ChatMessage msg;
		msg.id = fromStd(text.msg_id);
		msg.chatId = text.group_id.empty() ? fromStd(text.from_peer_id) : fromStd(text.group_id);
		msg.fromId = fromStd(text.from_peer_id);
		msg.fromName = fromStd(text.from_name);
		msg.text = fromStd(text.body);
		msg.timestampMs = qint64(text.timestamp_ms);
		msg.outgoing = false;
		msg.kind = emoji::isStickerPayload(msg.text)
			? ChatMessage::Kind::Sticker
			: ChatMessage::Kind::Text;
		if (text.group_id.empty()) {
			upsertPeer(msg.fromId, msg.fromName, true);
		}
		appendMessage(msg, msg.chatId != activeChat_);
		chatsDirty.insert(msg.chatId);
		dialogsDirty = true;
	}

	for (const auto &ack : node_->take_acks()) {
		const auto id = fromStd(ack);
		for (auto it = messages_.begin(); it != messages_.end(); ++it) {
			for (auto &msg : it.value()) {
				if (msg.outgoing && msg.id == id) {
					msg.acked = true;
					persist(it.key());
					chatsDirty.insert(it.key());
				}
			}
		}
	}

	for (const auto &event : node_->take_file_events()) {
		const auto peerId = fromStd(event.peer_id);
		const auto groupId = fromStd(event.group_id);
		const auto chatId = groupId.isEmpty() ? peerId : groupId;
		if (groupId.isEmpty() && !event.outgoing) {
			upsertPeer(peerId, dialog(peerId).name, true);
		}
		ChatMessage patch;
		patch.chatId = chatId;
		patch.fromId = event.outgoing ? selfId_ : peerId;
		patch.fromName = event.outgoing ? selfName_ : dialog(peerId).name;
		patch.outgoing = event.outgoing;
		patch.kind = event.is_folder ? ChatMessage::Kind::Folder : ChatMessage::Kind::File;
		patch.transferId = fromStd(event.transfer_id);
		patch.id = patch.transferId;
		patch.filename = fromStd(event.filename);
		patch.mime = fromStd(event.mime);
		patch.size = event.size;
		patch.bytesDone = event.bytes_done;
		patch.path = QString::fromStdString(event.path.u8string());
		if (event.error == "checksum mismatch") {
			patch.error = QStringLiteral("checksum_mismatch");
		} else if (event.error == "rejected") {
			patch.error = QStringLiteral("rejected");
		} else if (event.error == "cancelled") {
			patch.error = QStringLiteral("cancelled");
		} else if (event.error == "peer aborted the transfer"
			|| event.error == "peer aborted the folder") {
			patch.error = QStringLiteral("cancelled");
		} else if (!event.error.empty()) {
			patch.error = fromStd(event.error);
		}
		patch.failed = event.kind == lan::FileEvent::Kind::Failed;
		patch.complete = event.kind == lan::FileEvent::Kind::Completed;
		patch.pendingAccept = event.kind == lan::FileEvent::Kind::Offered;
		if (event.outgoing
			&& (event.kind == lan::FileEvent::Kind::Started
				|| event.kind == lan::FileEvent::Kind::Progress
				|| event.kind == lan::FileEvent::Kind::Completed
				|| event.kind == lan::FileEvent::Kind::Failed)) {
			patch.pendingAccept = false;
		}
		patch.verified = event.verified;
		if (event.kind == lan::FileEvent::Kind::Completed
			|| event.kind == lan::FileEvent::Kind::Progress
			|| event.kind == lan::FileEvent::Kind::Started
			|| event.kind == lan::FileEvent::Kind::Offered) {
			patch.timestampMs = nowMs();
		}
		const bool publish = should_publish_transfer_ui(
			event.kind == lan::FileEvent::Kind::Progress,
			lastTransferUiMs_.value(patch.transferId, 0),
			nowMs());
		updateTransfer(patch, true, publish);
		if (publish) {
			lastTransferUiMs_.insert(patch.transferId, nowMs());
			chatsDirty.insert(chatId);
			dialogsDirty = true;
			if (patch.complete || patch.failed) {
				lastTransferUiMs_.remove(patch.transferId);
			}
		}
	}

	for (const auto &event : node_->take_group_events()) {
		Dialog dialog;
		dialog.id = fromStd(event.group.group_id);
		dialog.isGroup = true;
		dialog.name = fromStd(event.group.name);
		dialog.creatorId = fromStd(event.group.creator_id);
		for (const auto &member : event.group.member_ids) {
			dialog.memberIds.push_back(fromStd(member));
		}
		if (event.kind == lan::GroupEvent::Kind::Removed) {
			dialogs_.erase(
				std::remove_if(
					dialogs_.begin(),
					dialogs_.end(),
					[&](const Dialog &item) { return item.id == dialog.id; }),
				dialogs_.end());
		} else {
			upsertGroup(dialog);
		}
		dialogsDirty = true;
		chatsDirty.insert(dialog.id);
	}

	for (const auto &recall : node_->take_recalls()) {
		const auto chatId = recall.group_id.empty()
			? fromStd(recall.from_peer_id)
			: fromStd(recall.group_id);
		auto &list = messages_[chatId];
		for (auto &msg : list) {
			if (msg.id == fromStd(recall.msg_id) && msg.fromId == fromStd(recall.from_peer_id)) {
				markDeleted(msg);
				touchDialog(chatId, previewOf(msg), msg.timestampMs);
				persist(chatId);
				chatsDirty.insert(chatId);
				dialogsDirty = true;
			}
		}
	}

	if (dialogsDirty) {
		emit dialogsChanged();
	}
	for (const auto &id : chatsDirty) {
		emit chatChanged(id);
	}
}

QString NodeController::selfFirstName() const { return selfFirst_; }
QString NodeController::selfLastName() const { return selfLast_; }
QString NodeController::selfBio() const { return selfBio_; }

QString NodeController::readProfileField(const char *name) const {
	const auto path = identityDir() / name;
	std::error_code ec;
	if (!std::filesystem::is_regular_file(path, ec)) {
		return {};
	}
	QFile file(QString::fromStdString(path.u8string()));
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	return QString::fromUtf8(file.readAll()).trimmed();
}

void NodeController::writeProfileField(const char *name, const QString &value) const {
	const auto path = identityDir() / name;
	QFile file(QString::fromStdString(path.u8string()));
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(value.toUtf8());
	}
}

void NodeController::applyProfileToNode() {
	if (!node_) {
		return;
	}
	node_->set_profile(toStd(selfFirst_), toStd(selfLast_), toStd(selfBio_));
}

void NodeController::setEncryptFiles(bool enabled) {
	if (node_) {
		node_->set_encrypt_files(enabled);
	}
}

bool NodeController::encryptFiles() const {
	return node_ && node_->encrypt_files();
}

PeerUpsertChange NodeController::upsertPeer(
		const QString &peerId,
		const QString &name,
		bool online,
		const QString &ip,
		const QString &firstName,
		const QString &lastName,
		const QString &bio) {
	PeerUpsertChange change;
	if (peerId.isEmpty() || peerId == selfId_) {
		return change;
	}
	for (auto &item : dialogs_) {
		if (item.id == peerId) {
			change = apply_peer_upsert(
				item, name, online, ip, firstName, lastName, bio);
			if (change.hintChanged) {
				persistPeerHints();
			}
			if (should_merge_offline_leftovers(change, online)) {
				mergeOfflineLeftovers(peerId);
				change.dialogChanged = true;
			}
			return change;
		}
	}
	Dialog dialog;
	dialog.id = peerId;
	dialog.name = name.isEmpty() ? peerId.left(8) : name;
	dialog.online = online;
	dialog.ip = ip;
	dialog.firstName = firstName;
	dialog.lastName = lastName;
	dialog.bio = bio;
	messages_[peerId] = history_->load(peerId);
	if (!messages_[peerId].isEmpty()) {
		const auto &last = messages_[peerId].last();
		dialog.lastText = previewOf(last);
		dialog.lastTime = last.timestampMs;
	}
	dialogs_.push_back(dialog);
	change.created = true;
	change.hintChanged = true;
	change.dialogChanged = true;
	change.cameOnline = online;
	if (!ip.isEmpty() || is_usable_display_name(dialog.id, dialog.name)) {
		change.identityBecameMergeable = true;
	}
	persistPeerHints();
	if (should_merge_offline_leftovers(change, online)) {
		mergeOfflineLeftovers(peerId);
	}
	return change;
}

void NodeController::persistPeerHints() {
	if (!history_) {
		return;
	}
	QVector<PeerHint> hints;
	for (const auto &item : dialogs_) {
		if (item.isGroup || item.id.isEmpty()) {
			continue;
		}
		PeerHint hint;
		hint.id = item.id;
		hint.name = item.name;
		hint.ip = item.ip;
		hints.push_back(hint);
	}
	if (peer_hints_equal(hints, lastSavedHints_)) {
		return;
	}
	history_->saveHints(hints);
	lastSavedHints_ = hints;
	++hintPersistCount_;
}

void NodeController::mergeOfflineLeftovers(const QString &liveId) {
	++leftoverMergeAttempts_;
	DialogBook book;
	book.selfId = selfId_;
	book.activeChat = activeChat_;
	book.dialogs = dialogs_;
	book.messages = messages_;
	const auto sources = merge_offline_leftovers_into(book, liveId);
	if (sources.isEmpty()) {
		return;
	}
	dialogs_ = book.dialogs;
	messages_ = book.messages;
	activeChat_ = book.activeChat;
	if (history_) {
		history_->save(liveId, messages_.value(liveId));
		for (const auto &src : sources) {
			history_->remove(src);
		}
	}
	for (auto it = transferToChat_.begin(); it != transferToChat_.end(); ++it) {
		if (sources.contains(it.value())) {
			it.value() = liveId;
		}
	}
	if (!messages_.value(liveId).isEmpty()) {
		const auto &last = messages_.value(liveId).last();
		touchDialog(liveId, previewOf(last), last.timestampMs);
	}
	persistPeerHints();
}

void NodeController::upsertGroup(const Dialog &group) {
	for (auto &item : dialogs_) {
		if (item.id == group.id) {
			item.name = group.name;
			item.memberIds = group.memberIds;
			item.creatorId = group.creatorId;
			item.isGroup = true;
			return;
		}
	}
	Dialog copy = group;
	if (!messages_.contains(copy.id)) {
		messages_[copy.id] = history_->load(copy.id);
		if (!messages_[copy.id].isEmpty()) {
			const auto &last = messages_[copy.id].last();
			copy.lastText = previewOf(last);
			copy.lastTime = last.timestampMs;
		}
	}
	dialogs_.push_back(copy);
}

void NodeController::appendMessage(ChatMessage message, bool incrementUnread) {
	auto &list = messages_[message.chatId];
	for (const auto &existing : list) {
		if (!message.id.isEmpty() && existing.id == message.id) {
			return;
		}
	}
	if (message.timestampMs == 0) {
		message.timestampMs = nowMs();
	}
	list.push_back(message);
	touchDialog(message.chatId, previewOf(message), message.timestampMs);
	if (incrementUnread) {
		for (auto &item : dialogs_) {
			if (item.id == message.chatId) {
				item.unread += 1;
			}
		}
	}
	persist(message.chatId);
	emit chatChanged(message.chatId);
	emit dialogsChanged();
}

void NodeController::updateTransfer(const ChatMessage &patch, bool createIfMissing, bool persistNow) {
	auto &list = messages_[patch.chatId];
	for (auto &msg : list) {
		const bool sameId = !patch.transferId.isEmpty()
			&& (msg.transferId == patch.transferId || msg.id == patch.transferId);
		const bool sameOutgoingName = msg.outgoing && patch.outgoing
			&& msg.kind == patch.kind
			&& msg.filename == patch.filename
			&& !msg.complete;
		if (sameId || sameOutgoingName) {
			if (!patch.transferId.isEmpty()) {
				msg.transferId = patch.transferId;
			}
			msg.filename = patch.filename.isEmpty() ? msg.filename : patch.filename;
			msg.mime = patch.mime.isEmpty() ? msg.mime : patch.mime;
			if (patch.size) {
				msg.size = patch.size;
			}
			msg.bytesDone = patch.bytesDone;
			if (!patch.path.isEmpty()) {
				msg.path = patch.path;
			}
			msg.error = patch.error;
			msg.failed = patch.failed;
			msg.complete = patch.complete || msg.complete;
			if (!patch.path.isEmpty() || patch.pendingAccept || patch.complete || patch.failed) {
				msg.pendingAccept = patch.pendingAccept && !patch.complete && !patch.failed;
			}
			if (patch.verified) {
				msg.verified = true;
			}
			if (msg.complete && !msg.failed) {
				msg.acked = true;
				msg.bytesDone = msg.size;
				msg.pendingAccept = false;
			}
			if (msg.failed) {
				msg.pendingAccept = false;
			}
			if (persistNow) {
				touchDialog(patch.chatId, previewOf(msg), msg.timestampMs ? msg.timestampMs : nowMs());
				persist(patch.chatId);
			}
			return;
		}
	}
	if (!createIfMissing) {
		return;
	}
	ChatMessage msg = patch;
	if (msg.id.isEmpty()) {
		msg.id = msg.transferId;
	}
	if (msg.timestampMs == 0) {
		msg.timestampMs = nowMs();
	}
	list.push_back(msg);
	touchDialog(msg.chatId, previewOf(msg), msg.timestampMs);
	if (!msg.outgoing && msg.chatId != activeChat_) {
		for (auto &item : dialogs_) {
			if (item.id == msg.chatId) {
				item.unread += 1;
			}
		}
	}
	persist(msg.chatId);
}

void NodeController::touchDialog(const QString &chatId, const QString &preview, qint64 time) {
	for (auto &item : dialogs_) {
		if (item.id == chatId) {
			item.lastText = preview;
			item.lastTime = time;
			return;
		}
	}
}

void NodeController::persist(const QString &chatId) {
	if (history_) {
		history_->save(chatId, messages_.value(chatId));
	}
}

qint64 NodeController::nowMs() const {
	return QDateTime::currentMSecsSinceEpoch();
}

std::filesystem::path NodeController::identityDir() const {
	return workingDir_ / "identity";
}

QString NodeController::selfAvatarPath() const {
	return cachedAvatarPath(selfId_);
}

void NodeController::markDeleted(ChatMessage &message) {
	const bool wasFile = message.kind == ChatMessage::Kind::File
		|| message.kind == ChatMessage::Kind::Folder;
	const QString path = message.path;
	const bool isFolder = message.kind == ChatMessage::Kind::Folder;
	const bool outgoing = message.outgoing;
	message.kind = ChatMessage::Kind::Deleted;
	message.text = QStringLiteral("message_deleted");
	message.filename.clear();
	message.mime.clear();
	message.failed = false;
	message.complete = true;
	message.pendingAccept = false;
	message.transferId.clear();
	// Receiver: also remove the saved payload. Sender paths are the
	// original source and must never be deleted.
	if (wasFile && !outgoing && !path.isEmpty()) {
		const auto status = lan::delete_recalled_payload(
			std::filesystem::u8path(path.toUtf8().constData()),
			isFolder);
		message.path.clear();
		if (status == lan::RecalledDeleteStatus::Locked) {
			message.text = QStringLiteral("recall_file_in_use");
		} else if (status == lan::RecalledDeleteStatus::Refused
			|| status == lan::RecalledDeleteStatus::None) {
			// AlreadyGone / Deleted are silent successes.
			if (status == lan::RecalledDeleteStatus::Refused) {
				message.text = QStringLiteral("recall_file_failed");
			}
		}
	} else {
		message.path.clear();
	}
}

void NodeController::sendSticker(const QString &chatId, const QString &stickerId) {
	if (stickerId.isEmpty()) {
		return;
	}
	sendText(chatId, emoji::payloadForSticker(stickerId));
	auto &list = messages_[chatId];
	if (!list.isEmpty() && list.last().outgoing && emoji::isStickerPayload(list.last().text)) {
		list.last().kind = ChatMessage::Kind::Sticker;
		touchDialog(chatId, previewOf(list.last()), list.last().timestampMs);
		persist(chatId);
		emit chatChanged(chatId);
		emit dialogsChanged();
	}
}

bool NodeController::setDisplayName(const QString &name) {
	if (!node_) {
		return false;
	}
	const auto normalized = lan::normalize_display_name(toStd(name));
	if (normalized.empty()) {
		return false;
	}
	auto identity = node_->identity();
	identity.display_name = normalized;
	try {
		lan::save_identity(identityDir(), identity);
	} catch (const std::exception &) {
		return false;
	}
	if (!node_->set_display_name(normalized)) {
		return false;
	}
	selfName_ = fromStd(normalized);
	emit selfReady(selfName_);
	return true;
}

bool NodeController::setProfile(const QString &firstName, const QString &lastName, const QString &bio) {
	selfFirst_ = firstName.trimmed();
	selfLast_ = lastName.trimmed();
	selfBio_ = bio.trimmed();
	writeProfileField("first_name", selfFirst_);
	writeProfileField("last_name", selfLast_);
	writeProfileField("bio", selfBio_);
	QString name = selfFirst_;
	if (!selfLast_.isEmpty()) {
		name = selfFirst_.isEmpty() ? selfLast_ : (selfFirst_ + QChar(' ') + selfLast_);
	}
	if (name.isEmpty()) {
		name = selfName_;
	}
	applyProfileToNode();
	return setDisplayName(name);
}

bool NodeController::setAvatarFromPath(const QString &imagePath) {
	if (imagePath.isEmpty()) {
		return false;
	}
	QImage image(imagePath);
	if (image.isNull()) {
		return false;
	}
	if (image.width() > 256 || image.height() > 256) {
		image = image.scaled(256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
	}
	const auto dest = identityDir() / "avatar.png";
	const QString destStr = QString::fromStdString(dest.u8string());
	if (!image.save(destStr, "PNG")) {
		return false;
	}
	setCachedAvatar(selfId_, destStr);
	emit selfReady(selfName_);
	return true;
}

void NodeController::setDownloadsDir(const std::filesystem::path &dir) {
	if (dir.empty()) {
		return;
	}
	downloadsDir_ = dir;
	std::filesystem::create_directories(downloadsDir_);
	if (node_) {
		node_->set_incoming_dir(downloadsDir_);
	}
}

bool NodeController::acceptTransfer(const QString &transferId, const QString &destPath) {
	if (!node_ || transferId.isEmpty()) {
		return false;
	}
	std::filesystem::path dest;
	if (!destPath.isEmpty()) {
		dest = std::filesystem::u8path(destPath.toUtf8().constData());
	}
	return node_->accept_transfer(toStd(transferId), dest);
}

bool NodeController::rejectTransfer(const QString &transferId) {
	if (!node_ || transferId.isEmpty()) {
		return false;
	}
	return node_->reject_transfer(toStd(transferId));
}

bool NodeController::abortTransfer(const QString &chatId, const QString &transferId) {
	if (!node_ || transferId.isEmpty()) {
		return false;
	}
	if (!node_->abort_transfer(toStd(transferId))) {
		return false;
	}
	auto &list = messages_[chatId];
	for (auto &msg : list) {
		if (msg.transferId == transferId || msg.id == transferId) {
			msg.failed = true;
			msg.error = QStringLiteral("cancelled");
			persist(chatId);
			emit chatChanged(chatId);
			break;
		}
	}
	return true;
}

bool NodeController::recallMessage(const QString &chatId, const QString &msgId) {
	if (!node_ || chatId.isEmpty() || msgId.isEmpty()) {
		return false;
	}
	auto &list = messages_[chatId];
	for (auto &msg : list) {
		if (msg.id != msgId) {
			continue;
		}
		if (!can_recall_message(msg, nowMs())) {
			return false;
		}
		const auto dialog = this->dialog(chatId);
		if (!node_->recall_message(toStd(chatId), toStd(msgId), dialog.isGroup, uint64_t(msg.timestampMs))) {
			return false;
		}
		if (transfer_in_progress(msg) && !msg.transferId.isEmpty()) {
			node_->abort_transfer(toStd(msg.transferId));
		}
		markDeleted(msg);
		touchDialog(chatId, previewOf(msg), msg.timestampMs);
		persist(chatId);
		emit chatChanged(chatId);
		emit dialogsChanged();
		return true;
	}
	return false;
}
