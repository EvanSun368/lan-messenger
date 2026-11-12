#include "popup_menu.h"

#include "icons.h"
#include "theme.h"

#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>

namespace {
// Official tdesktop dropdownMenuWithIcons / menuWithIcons / defaultMenu.
constexpr int kRadius = 8;
constexpr int kWidthMin = 156;
constexpr int kWidthMax = 300;
constexpr int kIconSize = 24;
constexpr int kIconX = 15;
constexpr int kIconY = 5;
constexpr int kTextLeft = 54;
constexpr int kPadTop = 8;
constexpr int kPadBottom = 8;
constexpr int kPadRight = 17;
constexpr int kDropdownShadow = 10;
constexpr int kDropdownScroll = 6;
constexpr int kContextShadow = 8;
constexpr int kContextScroll = 5;

void paintRoundShadow(QPainter &painter, const QRect &card, int pad, int radius) {
	// Approximate official roundShadowRadius8px (10px extend, heavier at the bottom).
	painter.save();
	painter.setBrush(Qt::NoBrush);
	for (int i = pad; i >= 1; --i) {
		const qreal t = qreal(i) / qreal(pad);
		const int alpha = qRound(28.0 * t * t);
		painter.setPen(QColor(0, 0, 0, alpha));
		const int extraBottom = (i > pad / 2) ? 1 : 0;
		const QRect r = card.adjusted(-i, -i, i, i + extraBottom);
		painter.drawRoundedRect(r, radius + i * 0.35, radius + i * 0.35);
	}
	painter.restore();
}
}

PopupMenu::PopupMenu(QWidget *parent)
	: QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) {
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_DeleteOnClose);
	setMouseTracking(true);
}

void PopupMenu::setKind(Kind kind) {
	kind_ = kind;
	if (kind_ == Kind::Dropdown) {
		shadowPad_ = kDropdownShadow;
		scrollPad_ = kDropdownScroll;
	} else {
		shadowPad_ = kContextShadow;
		scrollPad_ = kContextScroll;
	}
	relayout();
}

void PopupMenu::setItems(const QVector<Item> &items) {
	items_ = items;
	hover_ = -1;
	relayout();
	update();
}

QSize PopupMenu::contentSize() const {
	const auto &theme = Theme::current();
	const auto fm = QFontMetrics(theme.fontText);
	int width = kWidthMin;
	for (const auto &item : items_) {
		width = qMax(width, kTextLeft + fm.horizontalAdvance(item.text) + kPadRight);
	}
	width = qMin(width, kWidthMax);
	const int fontH = fm.height();
	const int itemH = qMax(kPadTop + fontH + kPadBottom, kIconY + kIconSize + 4);
	const int height = scrollPad_ * 2 + items_.size() * itemH;
	return QSize(width, height);
}

void PopupMenu::relayout() {
	const auto &theme = Theme::current();
	const auto fm = QFontMetrics(theme.fontText);
	itemHeight_ = qMax(kPadTop + fm.height() + kPadBottom, kIconY + kIconSize + 4);
	const QSize inner = contentSize();
	itemHeight_ = items_.isEmpty()
		? itemHeight_
		: (inner.height() - scrollPad_ * 2) / qMax(1, items_.size());
	setFixedSize(inner.width() + shadowPad_ * 2, inner.height() + shadowPad_ * 2);
}

QRect PopupMenu::cardRect() const {
	return rect().adjusted(shadowPad_, shadowPad_, -shadowPad_, -shadowPad_);
}

QRect PopupMenu::itemRect(int index) const {
	const QRect card = cardRect();
	return QRect(
		card.x(),
		card.y() + scrollPad_ + index * itemHeight_,
		card.width(),
		itemHeight_);
}

void PopupMenu::popup(const QPoint &globalPos) {
	relayout();
	QPoint pos = globalPos;
	if (auto *screen = QGuiApplication::screenAt(globalPos)) {
		const auto geo = screen->availableGeometry();
		const int margin = 8;
		if (pos.x() + width() > geo.right() - margin) {
			pos.setX(qMax(geo.left() + margin, geo.right() - width() - margin));
		}
		if (pos.x() < geo.left() + margin) {
			pos.setX(geo.left() + margin);
		}
		if (pos.y() + height() > geo.bottom() - margin) {
			pos.setY(qMax(geo.top() + margin, globalPos.y() - height() + shadowPad_));
		}
	}
	move(pos);
	show();
	raise();
}

void PopupMenu::popupAnchoredBottomLeft(const QPoint &globalOrigin) {
	relayout();
	// Official DropdownMenu: moveToLeft(0, buttonsBottom - height).
	// The widget includes the 10px shadow, so the visible card sits 10px
	// above the compose bar and 10px in from the chat pane's left edge.
	QPoint pos(globalOrigin.x(), globalOrigin.y() - height());
	if (auto *screen = QGuiApplication::screenAt(globalOrigin)) {
		const auto geo = screen->availableGeometry();
		if (pos.x() + width() > geo.right()) {
			pos.setX(qMax(geo.left(), geo.right() - width()));
		}
		if (pos.x() < geo.left()) {
			pos.setX(geo.left());
		}
		if (pos.y() < geo.top()) {
			// Not enough room above: open downward, still left-aligned.
			pos.setY(globalOrigin.y());
		}
	}
	move(pos);
	show();
	raise();
}

int PopupMenu::itemAt(const QPoint &pos) const {
	if (!cardRect().contains(pos)) {
		return -1;
	}
	const int y = pos.y() - cardRect().y() - scrollPad_;
	if (y < 0 || y >= items_.size() * itemHeight_) {
		return -1;
	}
	return y / itemHeight_;
}

void PopupMenu::paintEvent(QPaintEvent *) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRect card = cardRect();
	paintRoundShadow(painter, card, shadowPad_, kRadius);

	QPainterPath path;
	path.addRoundedRect(card, kRadius, kRadius);
	painter.fillPath(path, theme.boxBg);
	painter.setClipPath(path);

	for (int i = 0; i < items_.size(); ++i) {
		const auto &item = items_[i];
		const QRect row = itemRect(i);
		if (i == hover_) {
			painter.fillRect(row, theme.windowBgOver);
		}
		const auto color = item.danger ? theme.attentionFg : theme.windowFg;
		const auto iconColor = item.danger ? theme.attentionFg : theme.windowBoldFg;
		if (!item.icon.isEmpty()) {
			const auto icon = loadTintedIcon(item.icon, iconColor, kIconSize);
			painter.drawPixmap(row.x() + kIconX, row.y() + kIconY, icon);
		}
		painter.setPen(color);
		painter.setFont(theme.fontText);
		const QRect textRect(
			row.x() + kTextLeft,
			row.y() + kPadTop,
			row.width() - kTextLeft - kPadRight,
			row.height() - kPadTop - kPadBottom);
		painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, item.text);
	}
}

void PopupMenu::mouseMoveEvent(QMouseEvent *event) {
	const int hover = itemAt(event->pos());
	if (hover != hover_) {
		hover_ = hover;
		update();
	}
}

void PopupMenu::leaveEvent(QEvent *) {
	hover_ = -1;
	update();
}

void PopupMenu::mousePressEvent(QMouseEvent *event) {
	const int index = itemAt(event->pos());
	if (index < 0 || index >= items_.size()) {
		hide();
		return;
	}
	const auto action = items_[index].onClick;
	hide();
	if (action) {
		action();
	}
}

void PopupMenu::hideEvent(QHideEvent *) {
	hover_ = -1;
}

void PopupMenu::keyPressEvent(QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape) {
		hide();
	}
}
