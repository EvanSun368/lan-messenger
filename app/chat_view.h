#pragma once

#include "models.h"

#include <QWidget>
#include <QHash>
#include <QSet>
#include <memory>

class HistoryText;
class ComposeBar;
class EmojiPanel;
class QScrollArea;
class QDragEnterEvent;
class QDropEvent;

class MessageList : public QWidget {
	Q_OBJECT
public:
	explicit MessageList(QWidget *parent = nullptr);
	void setMessages(const QVector<ChatMessage> &messages, bool isGroup, const QString &selfId);
	void applyTheme();
	int contentHeight() const { return contentHeight_; }

signals:
	void openPath(const QString &path);
	void cancelTransfer(const QString &transferId);
	void recallMessage(const QString &msgId);
	void acceptTransfer(const QString &transferId);
	void rejectTransfer(const QString &transferId);
	void chooseSavePath(const QString &transferId, const QString &filename, bool isFolder);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private:
	struct LayoutItem {
		enum class Type { Date, Message };
		Type type = Type::Message;
		int y = 0;
		int height = 0;
		QRect bubble;
		QRect media;
		QRect cancel;
		QRect showMore;
		QRect accept;
		QRect decline;
		QRect saveAs;
		QRect time;
		QRect textRect;
		int messageIndex = -1;
		QString dateText;
	};

	void relayout();
	int maxBubbleWidth() const;
	int measureTextHeight(const QString &text, int width) const;
	struct TextMetrics {
		int height = 0;
		int fullHeight = 0;
		bool clipped = false;
		int lineH = 0;
	};
	TextMetrics measureBubbleText(const ChatMessage &msg, int width) const;
	void drawBubbleText(class QPainter *painter, const QRect &rect, const ChatMessage &msg, const class QColor &color, const QRect &clip) const;
	QPixmap photoOf(const ChatMessage &msg, int maxW, int maxH) const;
	int itemAt(const QPoint &pos) const;
	HistoryText &textFor(const ChatMessage &msg) const;
	QString textKey(const ChatMessage &msg) const;
	void pruneTextCache();

	QVector<ChatMessage> messages_;
	QVector<LayoutItem> items_;
	bool isGroup_ = false;
	QString selfId_;
	int contentHeight_ = 0;
	int cancelHover_ = -1;
	QSet<QString> expandedIds_;
	QSet<QString> revealedSpoilers_;
	mutable QHash<QString, std::shared_ptr<HistoryText>> historyText_;
};

class ChatView : public QWidget {
	Q_OBJECT
public:
	explicit ChatView(QWidget *parent = nullptr);

	void setSelf(const QString &id, const QString &name);
	void setDialog(const Dialog &dialog, const QVector<ChatMessage> &messages);
	void clearChat();
	void focusCompose();
	void clearCompose();
	void applyTheme();
	QRect membersHotspot() const;

signals:
	void sendText(const QString &text);
	void attachFile();
	void attachFolder();
	void attachPhoto();
	void showPeerInfo();
	void sendPaths(const QStringList &paths);
	void cancelTransfer(const QString &transferId);
	void recallMessage(const QString &msgId);
	void acceptTransfer(const QString &transferId);
	void rejectTransfer(const QString &transferId);
	void chooseSavePath(const QString &transferId, const QString &filename, bool isFolder);
	void editMembers();

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	void layoutChat();
	void scrollToBottom();
	void toggleEmoji();
	bool acceptDrop(const class QMimeData *data) const;

	QString selfId_;
	QString selfName_;
	Dialog dialog_;
	bool hasChat_ = false;
	MessageList *list_ = nullptr;
	QScrollArea *scroll_ = nullptr;
	ComposeBar *compose_ = nullptr;
	EmojiPanel *emoji_ = nullptr;
};
