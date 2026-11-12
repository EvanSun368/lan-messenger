#pragma once

#include <QDialog>
#include <QString>
#include <QVBoxLayout>

class QPushButton;
class QWidget;

// Official tdesktop Ui::BoxContent / BoxLayerWidget look: a frameless
// layer over the parent window with st::layerBg dim, an 8px rounded
// card, 16px semibold title, close icon, and text buttons on the right
// (confirm is rightmost). Click the dim or press Escape to close.
class OfficialBox : public QDialog {
	Q_OBJECT
public:
	explicit OfficialBox(QWidget *parent = nullptr);

	void setBoxTitle(const QString &title);
	void setBoxWidth(int width);
	QWidget *body() const { return body_; }
	QVBoxLayout *bodyLayout() const { return bodyLayout_; }

	QPushButton *addConfirmButton(const QString &text);
	QPushButton *addCancelButton(const QString &text);
	QPushButton *addCloseButton(const QString &text);

protected:
	void paintEvent(QPaintEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void applyChrome();
	void layoutLayer();
	void styleTextButton(QPushButton *button, bool attention = false);
	QRect cardRect() const;
	QRect closeRect() const;

	QWidget *card_ = nullptr;
	QWidget *body_ = nullptr;
	QVBoxLayout *bodyLayout_ = nullptr;
	QWidget *buttons_ = nullptr;
	QString title_;
	int boxWidth_ = 364;
	int shadow_ = 16;
	bool closeHover_ = false;
};
