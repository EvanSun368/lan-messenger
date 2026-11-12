#pragma once

#include <QTextEdit>
#include <QWidget>

// ChatView must reserve the bar's real height, not QWidget::sizeHint()
// (which is invalid / -1 when the bar has no QLayout). Using the 52 px
// fallback after the field has grown leaves the buttons below the window
// and clips the field to about two visible lines.
inline int reservedComposeHeight(bool hasChat, int barHeight, int minHeight) {
	if (!hasChat) {
		return 0;
	}
	return barHeight > 0 ? barHeight : minHeight;
}


class ComposeEdit : public QTextEdit {
	Q_OBJECT
public:
	explicit ComposeEdit(QWidget *parent = nullptr);
	void applyTheme();
	void syncPlaceholder();
	bool hasVisibleText() const;

protected:
	void inputMethodEvent(QInputMethodEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	bool composing_ = false;
};

class ComposeBar : public QWidget {
	Q_OBJECT
public:
	explicit ComposeBar(QWidget *parent = nullptr);
	void setEnabledChat(bool enabled);
	void focusEdit();
	void insertText(const QString &text);
	void clearDraft();
	void applyTheme();
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

signals:
	void sendText(const QString &text);
	void attachFile();
	void attachFolder();
	void attachPhoto();
	void emojiToggled();
	void heightAdjusted();

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QRect attachRect() const;
	QRect emojiRect() const;
	QRect sendRect() const;
	int fieldContentHeight(int fieldWidth) const;
	void relayout();
	void trySend();

	ComposeEdit *edit_ = nullptr;
	bool enabled_ = false;
	bool sendHover_ = false;
	bool attachHover_ = false;
	bool emojiHover_ = false;
	bool inHeightCheck_ = false;
};
