#include "emoji_panel.h"

#include "emoji_catalog.h"
#include "icons.h"
#include "theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>

namespace {

QStringList loadRecent() {
	QSettings settings;
	return settings.value(QStringLiteral("emoji/recent")).toStringList();
}

void saveRecent(const QStringList &recent) {
	QSettings settings;
	settings.setValue(QStringLiteral("emoji/recent"), recent);
}

} // namespace

EmojiPanel::EmojiPanel(QWidget *parent)
	: QWidget(parent)
	, recent_(loadRecent()) {
	setFixedSize(384, 340);
	setWindowFlags(Qt::Popup);
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	scroll_ = new QScrollArea(this);
	scroll_->setWidgetResizable(true);
	scroll_->setFrameShape(QFrame::NoFrame);
	scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	grid_ = new QWidget;
	scroll_->setWidget(grid_);
	layout->addWidget(scroll_, 1);

	auto *tabs = new QWidget(this);
	tabs->setFixedHeight(40);
	auto *tabLayout = new QHBoxLayout(tabs);
	tabLayout->setContentsMargins(8, 4, 8, 4);
	tabLayout->setSpacing(4);

	const struct TabSpec { Tab tab; const char *icon; } specs[] = {
		{Tab::Recent, ":/icons/emoji_recent.png"},
		{Tab::People, ":/icons/emoji_smile.png"},
		{Tab::Nature, ":/icons/emoji_nature.png"},
		{Tab::Food, ":/icons/emoji_food.png"},
		{Tab::Activity, ":/icons/emoji_activities.png"},
		{Tab::Travel, ":/icons/emoji_travel.png"},
		{Tab::Objects, ":/icons/emoji_objects.png"},
		{Tab::Symbols, ":/icons/emoji_symbols.png"},
		{Tab::Stickers, ":/icons/stickers.png"},
	};
	for (const auto &spec : specs) {
		auto *button = new QToolButton(tabs);
		button->setAutoRaise(true);
		button->setFixedSize(32, 32);
		button->setIconSize(QSize(20, 20));
		button->setProperty("tab", int(spec.tab));
		connect(button, &QToolButton::clicked, this, [this, tab = spec.tab]() { setTab(tab); });
		button->setProperty("iconPath", QString::fromUtf8(spec.icon));
		tabLayout->addWidget(button);
	}
	layout->addWidget(tabs);
	connect(&Theme::notifier(), &ThemeNotifier::changed, this, &EmojiPanel::applyTheme);
	applyTheme();
	rebuild();
}

void EmojiPanel::applyTheme() {
	const auto &theme = Theme::current();
	setStyleSheet(
		QStringLiteral("EmojiPanel { background: %1; border: 1px solid %2; }")
			.arg(theme.boxBg.name(), theme.inputBorderFg.name()));
	scroll_->setStyleSheet(
		QStringLiteral("QScrollArea { background: %1; border: none; }")
			.arg(theme.boxBg.name()));
	grid_->setStyleSheet(QStringLiteral("background: %1;").arg(theme.boxBg.name()));
	const auto buttons = findChildren<QToolButton *>();
	for (auto *button : buttons) {
		const auto path = button->property("iconPath").toString();
		const bool active = button->property("tab").toInt() == int(tab_);
		const auto color = active ? theme.windowBgActive : theme.composeIconFg;
		button->setIcon(loadTintedQIcon(path, color, 20));
	}
	update();
}

void EmojiPanel::setTab(Tab tab) {
	tab_ = tab;
	applyTheme();
	rebuild();
}

void EmojiPanel::addRecent(const QString &text) {
	recent_.removeAll(text);
	recent_.prepend(text);
	while (recent_.size() > 32) {
		recent_.removeLast();
	}
	saveRecent(recent_);
}

void EmojiPanel::rebuild() {
	if (grid_->layout()) {
		QLayoutItem *child = nullptr;
		while ((child = grid_->layout()->takeAt(0)) != nullptr) {
			delete child->widget();
			delete child;
		}
		delete grid_->layout();
	}
	auto *layout = new QGridLayout(grid_);
	layout->setContentsMargins(8, 8, 8, 16);
	layout->setSpacing(2);

	if (tab_ == Tab::Stickers) {
		const int cols = 4;
		for (int i = 0; i < emoji::kStickerCount; ++i) {
			const auto &item = emoji::kStickers[i];
			auto *button = new QToolButton(grid_);
			button->setAutoRaise(true);
			button->setIconSize(QSize(72, 72));
			button->setFixedSize(84, 84);
			button->setIcon(QIcon(emoji::stickerPixmap(QString::fromUtf8(item.id), 72)));
			const auto id = QString::fromUtf8(item.id);
			connect(button, &QToolButton::clicked, this, [this, id]() {
				emit stickerChosen(id);
				hide();
			});
			layout->addWidget(button, i / cols, i % cols);
		}
	} else {
		const int cols = 8;
		int count = 0;
		auto addEmoji = [&](const QString &text, int index) {
			auto *button = new QToolButton(grid_);
			button->setAutoRaise(true);
			button->setIconSize(QSize(32, 32));
			button->setFixedSize(40, 40);
			const auto pix = emoji::sprite(index, 32);
			if (!pix.isNull()) {
				button->setIcon(QIcon(pix));
			} else {
				button->setText(text);
			}
			connect(button, &QToolButton::clicked, this, [this, text]() {
				addRecent(text);
				emit emojiChosen(text);
				hide();
			});
			layout->addWidget(button, count / cols, count % cols);
			++count;
		};
		if (tab_ == Tab::Recent) {
			for (const auto &text : recent_) {
				int index = -1;
				for (int i = 0; i < emoji::kEmojiCount; ++i) {
					if (QString::fromUtf8(emoji::kEmoji[i].utf8) == text) {
						index = emoji::kEmoji[i].index;
						break;
					}
				}
				addEmoji(text, index);
			}
		} else {
			const int section = int(tab_) - 1; // People=0
			for (int i = 0; i < emoji::kEmojiCount; ++i) {
				if (emoji::kEmoji[i].section == section) {
					addEmoji(QString::fromUtf8(emoji::kEmoji[i].utf8), emoji::kEmoji[i].index);
				}
			}
		}
	}
	layout->setRowMinimumHeight(layout->rowCount(), 8);
	grid_->adjustSize();
	scroll_->verticalScrollBar()->setValue(0);
}

void EmojiPanel::paintEvent(QPaintEvent *) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	painter.fillRect(rect(), theme.boxBg);
	painter.setPen(theme.inputBorderFg);
	painter.drawRect(rect().adjusted(0, 0, -1, -1));
}
