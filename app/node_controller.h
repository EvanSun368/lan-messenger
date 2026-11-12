#pragma once

#include "models.h"
#include "step_policy.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
class QTimer;

namespace lan {
class Node;
struct Identity;
}

class HistoryStore;

class NodeController : public QObject {
	Q_OBJECT
public:
	struct NetworkOptions {
		std::string bindAddress = "0.0.0.0";
		std::string multicastGroup = "239.255.87.42";
		quint16 discoveryPort = 48710;
		quint16 udpPort = 0;
		quint16 tcpPort = 0;
		std::chrono::milliseconds announceInterval{1000};
		std::chrono::milliseconds peerTimeout{4000};
	};

	explicit NodeController(
		std::filesystem::path workingDir,
		std::string displayName = {},
		QObject *parent = nullptr);
	NodeController(
		std::filesystem::path workingDir,
		std::string displayName,
		NetworkOptions network,
		QObject *parent = nullptr);
	~NodeController() override;

	[[nodiscard]] QString selfId() const;
	[[nodiscard]] QString selfName() const;
	[[nodiscard]] QString selfFirstName() const;
	[[nodiscard]] QString selfLastName() const;
	[[nodiscard]] QString selfBio() const;
	[[nodiscard]] std::filesystem::path workingDir() const;
	[[nodiscard]] std::filesystem::path downloadsDir() const;
	[[nodiscard]] quint16 udpPort() const;
	void addDiscoveryTarget(const QString &ip, quint16 port);

	[[nodiscard]] QVector<Dialog> dialogs() const;
	[[nodiscard]] QVector<ChatMessage> messages(const QString &chatId) const;
	[[nodiscard]] Dialog dialog(const QString &chatId) const;
	[[nodiscard]] QVector<Dialog> onlinePeers() const;

	void setActiveChat(const QString &chatId);
	[[nodiscard]] QString activeChat() const { return activeChat_; }

	bool sendText(const QString &chatId, const QString &text);
	void sendSticker(const QString &chatId, const QString &stickerId);
	void sendPath(const QString &chatId, const QString &path);
	QString createGroup(const QString &name, const QStringList &memberIds);
	bool addGroupMember(const QString &groupId, const QString &peerId);
	bool removeGroupMember(const QString &groupId, const QString &peerId);
	bool setDisplayName(const QString &name);
	bool setProfile(const QString &firstName, const QString &lastName, const QString &bio);
	bool setAvatarFromPath(const QString &imagePath);
	void setEncryptFiles(bool enabled);
	[[nodiscard]] bool encryptFiles() const;
	void setDownloadsDir(const std::filesystem::path &dir);
	bool acceptTransfer(const QString &transferId, const QString &destPath = {});
	bool rejectTransfer(const QString &transferId);
	[[nodiscard]] QString selfAvatarPath() const;
	bool abortTransfer(const QString &chatId, const QString &transferId);
	bool recallMessage(const QString &chatId, const QString &msgId);

	// Observed by tests: actual hint file writes, leftover-merge attempts.
	[[nodiscard]] int hintPersistCount() const { return hintPersistCount_; }
	[[nodiscard]] int leftoverMergeAttempts() const { return leftoverMergeAttempts_; }

signals:
	void dialogsChanged();
	void chatChanged(const QString &chatId);
	void selfReady(const QString &name);

private:
	void step();
	PeerUpsertChange upsertPeer(
		const QString &peerId,
		const QString &name,
		bool online,
		const QString &ip = {},
		const QString &firstName = {},
		const QString &lastName = {},
		const QString &bio = {});
	QString readProfileField(const char *name) const;
	void writeProfileField(const char *name, const QString &value) const;
	void applyProfileToNode();
	void upsertGroup(const Dialog &group);
	void appendMessage(ChatMessage message, bool incrementUnread);
	void updateTransfer(const ChatMessage &patch, bool createIfMissing, bool persistNow = true);
	void touchDialog(const QString &chatId, const QString &preview, qint64 time);
	void persist(const QString &chatId);
	qint64 nowMs() const;
	void markDeleted(ChatMessage &message);
	void appendOfflineNotice(const QString &chatId);
	void persistPeerHints();
	void mergeOfflineLeftovers(const QString &liveId);
	std::filesystem::path identityDir() const;

	std::filesystem::path workingDir_;
	std::filesystem::path downloadsDir_;
	std::unique_ptr<lan::Node> node_;
	std::unique_ptr<HistoryStore> history_;
	QString selfId_;
	QString selfName_;
	QString selfFirst_;
	QString selfLast_;
	QString selfBio_;
	QString activeChat_;
	QVector<Dialog> dialogs_;
	QHash<QString, QVector<ChatMessage>> messages_;
	QHash<QString, QString> transferToChat_;
	QTimer *timer_ = nullptr;
	QVector<PeerHint> lastSavedHints_;
	QHash<QString, qint64> lastTransferUiMs_;
	int hintPersistCount_ = 0;
	int leftoverMergeAttempts_ = 0;
};
