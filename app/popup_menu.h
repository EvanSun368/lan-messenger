#pragma once

#include <QWidget>
#include <functional>
#include <QVector>
#include <QString>

class PopupMenu : public QWidget {
	Q_OBJECT
public:
	struct Item {
		QString id;
		QString text;
		QString icon;
		bool danger = false;
		std::function<void()> onClick;
	};

	// Context is official popupMenuWithIcons (cursor menus).
	// Dropdown is official dropdownMenuWithIcons (attach panel).
	enum class Kind { Context, Dropdown };

	explicit PopupMenu(QWidget *parent = nullptr);
	void setKind(Kind kind);
	void setItems(const QVector<Item> &items);
	void popup(const QPoint &globalPos);
	// Official attach menu: BottomLeft origin. globalOrigin is the
	// top-left of the chat compose strip (history x=0, buttonsBottom).
	void popupAnchoredBottomLeft(const QPoint &globalOrigin);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	QRect cardRect() const;
	QRect itemRect(int index) const;
	int itemAt(const QPoint &pos) const;
	QSize contentSize() const;
	void relayout();

	QVector<Item> items_;
	Kind kind_ = Kind::Context;
	int hover_ = -1;
	int itemHeight_ = 32;
	int shadowPad_ = 8;
	int scrollPad_ = 5;
};
