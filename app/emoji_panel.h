#pragma once

#include <QWidget>

class QScrollArea;

class EmojiPanel : public QWidget {
	Q_OBJECT
public:
	explicit EmojiPanel(QWidget *parent = nullptr);
	void applyTheme();

signals:
	void emojiChosen(const QString &text);
	void stickerChosen(const QString &id);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	enum class Tab { Recent, People, Nature, Food, Activity, Travel, Objects, Symbols, Stickers };

	void setTab(Tab tab);
	void rebuild();
	void addRecent(const QString &text);

	QWidget *grid_ = nullptr;
	QScrollArea *scroll_ = nullptr;
	Tab tab_ = Tab::People;
	QStringList recent_;
};
