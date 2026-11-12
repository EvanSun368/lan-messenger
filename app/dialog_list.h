#pragma once

#include "models.h"

#include "i18n.h"

inline QString dialogPreviewText(const Dialog &item) {
	if (!item.lastText.isEmpty()) {
		if (!item.isGroup && !item.online) {
			return i18n::tr("offline") + QStringLiteral(" · ") + item.lastText;
		}
		return item.lastText;
	}
	if (item.isGroup) {
		return i18n::tr("no_messages_yet");
	}
	return item.online ? i18n::tr("on_lan") : i18n::tr("offline");
}


#include <QWidget>

class QLineEdit;

class DialogList : public QWidget {
	Q_OBJECT
public:
	explicit DialogList(QWidget *parent = nullptr);

	void setDialogs(const QVector<Dialog> &dialogs);
	void setSelected(const QString &chatId);
	void setSelf(const QString &id, const QString &name);
	void applyTheme();

signals:
	void chatSelected(const QString &chatId);
	void menuRequested();

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private:
	int headerHeight() const;
	int listTop() const;
	int rowAt(const QPoint &pos) const;
	QVector<Dialog> filtered() const;
	void updateScroll();

	QLineEdit *search_ = nullptr;
	QVector<Dialog> dialogs_;
	QString selected_;
	QString selfId_;
	QString selfName_;
	int hover_ = -1;
	int scroll_ = 0;
};
