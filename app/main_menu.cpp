#include "main_menu.h"

#include "avatar.h"
#include "icons.h"
#include "theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

MainMenuPanel::MainMenuPanel(QWidget *parent)
	: QWidget(parent) {
	setMouseTracking(true);
	setAttribute(Qt::WA_OpaquePaintEvent);
}

int MainMenuPanel::officialWidth() const {
	return Theme::current().mainMenuWidth;
}

void MainMenuPanel::setIdentity(const QString &id, const QString &name, const QString &status) {
	selfId_ = id;
	selfName_ = name;
	status_ = status;
	update();
}

void MainMenuPanel::setItems(const QVector<Item> &items) {
	items_ = items;
	hover_ = -1;
	update();
}

void MainMenuPanel::setVersionText(const QString &product, const QString &versionLine) {
	product_ = product;
	versionLine_ = versionLine;
	update();
}

int MainMenuPanel::firstItemY() const {
	const auto &theme = Theme::current();
	return theme.mainMenuCoverHeight + theme.mainMenuSkip;
}

QRect MainMenuPanel::itemRect(int index) const {
	const auto &theme = Theme::current();
	int y = firstItemY();
	for (int i = 0; i < index && i < items_.size(); ++i) {
		if (items_[i].separatorBefore && i != 0) {
			y += theme.mainMenuSkip * 2 + 1;
		}
		y += theme.mainMenuItemHeight;
	}
	if (index >= 0 && index < items_.size() && items_[index].separatorBefore && index != 0) {
		y += theme.mainMenuSkip * 2 + 1;
	}
	return QRect(0, y, width(), theme.mainMenuItemHeight);
}

QRect MainMenuPanel::toggleRect(int index) const {
	const QRect row = itemRect(index);
	// Official toggle: 16px knob + 14px track, skip 19px from the right.
	return QRect(row.right() - 19 - 32, row.center().y() - 10, 32, 20);
}

int MainMenuPanel::itemAt(const QPoint &pos) const {
	for (int i = 0; i < items_.size(); ++i) {
		if (itemRect(i).contains(pos)) {
			return i;
		}
	}
	return -1;
}

void MainMenuPanel::paintEvent(QPaintEvent *) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), theme.windowBg);

	const QRect cover(0, 0, width(), theme.mainMenuCoverHeight);
	painter.fillRect(cover, theme.dialogsBgActive);

	const auto avatar = makeAvatar(selfId_, selfName_, theme.mainMenuUserpic);
	painter.drawPixmap(theme.mainMenuUserpicLeft, theme.mainMenuUserpicTop, avatar);

	QFont nameFont = theme.fontMenuItem;
	painter.setFont(nameFont);
	painter.setPen(theme.mainMenuCoverFg);
	const QRect nameRect(
		theme.mainMenuCoverNameLeft,
		theme.mainMenuCoverNameTop,
		width() - theme.mainMenuCoverNameLeft - 16,
		QFontMetrics(nameFont).height());
	painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
		QFontMetrics(nameFont).elidedText(selfName_, Qt::ElideRight, nameRect.width()));

	painter.setFont(theme.fontStatus);
	QColor statusFg = theme.mainMenuCoverFg;
	statusFg.setAlpha(200);
	painter.setPen(statusFg);
	const QRect statusRect(
		theme.mainMenuCoverNameLeft,
		theme.mainMenuCoverStatusTop,
		width() - theme.mainMenuCoverNameLeft - 16,
		QFontMetrics(theme.fontStatus).height());
	painter.drawText(statusRect, Qt::AlignLeft | Qt::AlignVCenter, status_);

	// Hairline under the cover, official PlainShadow.
	painter.fillRect(QRect(0, theme.mainMenuCoverHeight - 1, width(), 1),
		theme.dark ? QColor(0, 0, 0, 60) : QColor(0, 0, 0, 24));

	for (int i = 0; i < items_.size(); ++i) {
		const auto &item = items_[i];
		const QRect row = itemRect(i);
		if (item.separatorBefore && i != 0) {
			const int sy = row.y() - theme.mainMenuSkip - 1;
			painter.fillRect(QRect(0, sy, width(), 1), theme.dialogsDivider);
		}
		if (hover_ == i) {
			painter.fillRect(row, theme.windowBgOver);
		}
		const auto icon = loadTintedIcon(item.icon, theme.menuIconFg, 24);
		if (!icon.isNull()) {
			const int iy = row.y() + (row.height() - icon.height()) / 2;
			painter.drawPixmap(theme.mainMenuIconLeft, iy, icon);
		}
		painter.setFont(theme.fontMenuItem);
		painter.setPen(theme.windowBoldFg);
		const int textRight = item.toggle ? toggleRect(i).x() - 8 : row.right() - 20;
		const QRect textRect(theme.mainMenuTextLeft, row.y(), textRight - theme.mainMenuTextLeft, row.height());
		painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, item.text);

		if (item.toggle) {
			const QRect tr = toggleRect(i);
			const bool on = item.toggled;
			QPainterPath track;
			track.addRoundedRect(tr, tr.height() / 2.0, tr.height() / 2.0);
			painter.fillPath(track, on ? theme.windowBgActive : (theme.dark ? QColor("#3e546a") : QColor("#cccccc")));
			const int knob = tr.height() - 4;
			const int kx = on ? tr.right() - 2 - knob : tr.x() + 2;
			painter.setPen(Qt::NoPen);
			painter.setBrush(QColor("#ffffff"));
			painter.drawEllipse(QRect(kx, tr.y() + 2, knob, knob));
		}
	}

	// Footer, official mainMenuTelegramLabel / Version – About
	const int footerTop = items_.isEmpty()
		? firstItemY()
		: itemRect(items_.size() - 1).bottom() + 8;
	const int footerMin = theme.mainMenuFooterHeightMin;
	const int footerY = qMax(footerTop, height() - footerMin);
	painter.setFont(theme.fontMenuItem);
	painter.setPen(theme.windowSubTextFg);
	const int productH = QFontMetrics(theme.fontMenuItem).height();
	painter.drawText(
		QRect(theme.mainMenuFooterLeft, height() - 38 - productH, width() - 32, productH),
		Qt::AlignLeft | Qt::AlignVCenter,
		product_);
	painter.setFont(theme.fontStatus);
	painter.drawText(
		QRect(theme.mainMenuFooterLeft, height() - 17 - QFontMetrics(theme.fontStatus).height(),
			width() - 32, QFontMetrics(theme.fontStatus).height()),
		Qt::AlignLeft | Qt::AlignVCenter,
		versionLine_);
	Q_UNUSED(footerY);
}

void MainMenuPanel::mouseMoveEvent(QMouseEvent *event) {
	const int hover = itemAt(event->pos());
	if (hover != hover_) {
		hover_ = hover;
		update();
	}
}

void MainMenuPanel::leaveEvent(QEvent *) {
	if (hover_ != -1) {
		hover_ = -1;
		update();
	}
}

void MainMenuPanel::mousePressEvent(QMouseEvent *event) {
	if (event->button() != Qt::LeftButton) {
		return;
	}
	// Version line "About" click target: official treats the second
	// half of the version footer as the About link.
	const auto &theme = Theme::current();
	const QRect aboutRect(
		theme.mainMenuFooterLeft,
		height() - 17 - QFontMetrics(theme.fontStatus).height(),
		width() - 32,
		QFontMetrics(theme.fontStatus).height());
	if (aboutRect.contains(event->pos())) {
		emit aboutClicked();
		return;
	}
	const int index = itemAt(event->pos());
	if (index < 0 || index >= items_.size()) {
		return;
	}
	if (items_[index].onClick) {
		items_[index].onClick();
	}
}

void MainMenuPanel::resizeEvent(QResizeEvent *) {
	update();
}
