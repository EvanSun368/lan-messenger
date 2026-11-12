#pragma once

#include <QWidget>
#include <functional>

class QPaintEvent;
class QMouseEvent;
class QResizeEvent;

// Official Window::MainMenu look: 274px panel, 134px blue cover,
// 48px userpic, 40px icon rows, night-mode toggle, footer version.
class MainMenuPanel : public QWidget {
	Q_OBJECT
public:
	struct Item {
		QString id;
		QString text;
		QString icon;
		bool toggle = false;
		bool toggled = false;
		bool separatorBefore = false;
		std::function<void()> onClick;
	};

	explicit MainMenuPanel(QWidget *parent = nullptr);

	void setIdentity(const QString &id, const QString &name, const QString &status);
	void setItems(const QVector<Item> &items);
	void setVersionText(const QString &product, const QString &versionLine);

	int officialWidth() const;

signals:
	void aboutClicked();
	void nightToggled(bool night);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	QRect itemRect(int index) const;
	QRect toggleRect(int index) const;
	int itemAt(const QPoint &pos) const;
	int firstItemY() const;

	QString selfId_;
	QString selfName_;
	QString status_;
	QString product_;
	QString versionLine_;
	QVector<Item> items_;
	int hover_ = -1;
};
