#include "dialog_list.h"

#include "avatar.h"
#include "icons.h"
#include "theme.h"
#include "i18n.h"

#include <QDateTime>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

namespace {

QString formatDialogTime(qint64 ms) {
	if (ms <= 0) {
		return {};
	}
	const auto dt = QDateTime::fromMSecsSinceEpoch(ms);
	const auto today = QDate::currentDate();
	if (dt.date() == today) {
		return dt.toString(QStringLiteral("hh:mm"));
	}
	if (dt.date() == today.addDays(-1)) {
		return i18n::tr("yesterday");
	}
	if (dt.date().year() == today.year()) {
		return dt.toString(QStringLiteral("MMM d"));
	}
	return dt.toString(QStringLiteral("dd.MM.yyyy"));
}

} // namespace

DialogList::DialogList(QWidget *parent)
	: QWidget(parent) {
	const auto &theme = Theme::current();
	setFixedWidth(theme.dialogsWidth);
	setMouseTracking(true);
	search_ = new QLineEdit(this);
	search_->setPlaceholderText(i18n::tr("search"));
	search_->setFrame(false);
	search_->setFont(theme.fontPreview);
	search_->setFixedHeight(36);
	connect(search_, &QLineEdit::textChanged, this, [this](const QString &) { update(); });
	connect(&Theme::notifier(), &ThemeNotifier::changed, this, &DialogList::applyTheme);
	connect(&i18n::notifier(), &LanguageNotifier::changed, this, [this]() {
		search_->setPlaceholderText(i18n::tr("search"));
		update();
	});
	applyTheme();
}

void DialogList::applyTheme() {
	resizeEvent(nullptr);
	update();
}

void DialogList::setDialogs(const QVector<Dialog> &dialogs) {
	dialogs_ = dialogs;
	updateScroll();
	update();
}

void DialogList::setSelected(const QString &chatId) {
	selected_ = chatId;
	update();
}

void DialogList::setSelf(const QString &id, const QString &name) {
	selfId_ = id;
	selfName_ = name;
	update();
}

int DialogList::headerHeight() const {
	return 56;
}

int DialogList::listTop() const {
	return headerHeight();
}

int DialogList::rowAt(const QPoint &pos) const {
	if (pos.y() < listTop()) {
		return -1;
	}
	const auto rows = filtered();
	const int y = pos.y() - listTop() + scroll_;
	const int index = y / Theme::current().dialogsRowHeight;
	if (index < 0 || index >= rows.size()) {
		return -1;
	}
	return index;
}

QVector<Dialog> DialogList::filtered() const {
	const auto query = search_->text().trimmed();
	if (query.isEmpty()) {
		return dialogs_;
	}
	QVector<Dialog> out;
	for (const auto &item : dialogs_) {
		if (item.name.contains(query, Qt::CaseInsensitive)
			|| item.lastText.contains(query, Qt::CaseInsensitive)) {
			out.push_back(item);
		}
	}
	return out;
}

void DialogList::updateScroll() {
	const auto rows = filtered();
	const int total = rows.size() * Theme::current().dialogsRowHeight;
	const int view = qMax(0, height() - listTop());
	scroll_ = qBound(0, scroll_, qMax(0, total - view));
}

void DialogList::resizeEvent(QResizeEvent *) {
	const int menuPad = 52;
	search_->setGeometry(menuPad, 10, width() - menuPad - 14, 36);
	const auto &theme = Theme::current();
	search_->setStyleSheet(
		QStringLiteral(
			"QLineEdit { background: %1; color: %2; border: none; border-radius: 18px;"
			" padding-left: 36px; padding-right: 12px; selection-background-color: %3; }")
			.arg(theme.filterInputBg.name(), theme.windowFg.name(), theme.windowBgActive.name()));
	updateScroll();
}

void DialogList::paintEvent(QPaintEvent *) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), theme.dialogsBg);

	// Header: official dialogs menu icon + search (search is a child widget).
	QRect menuRect(8, 10, 36, 36);
	const auto menuIcon = loadTintedIcon(
		QStringLiteral(":/icons/dialogs_menu.png"),
		theme.menuIconFg,
		20);
	if (!menuIcon.isNull() && menuIcon.width() > 4) {
		painter.drawPixmap(menuRect.center() - QPoint(menuIcon.width() / 2, menuIcon.height() / 2), menuIcon);
	} else {
		paintHamburger(&painter, menuRect, theme.menuIconFg);
	}

	const auto searchIcon = loadTintedIcon(
		QStringLiteral(":/icons/search.png"),
		theme.placeholderFg,
		18);
	const int iconX = search_->x() + 12;
	const int iconY = search_->y() + (search_->height() - 18) / 2;
	painter.drawPixmap(iconX, iconY, searchIcon);

	painter.fillRect(QRect(width() - 1, 0, 1, height()), theme.dialogsDivider);

	const auto rows = filtered();
	painter.setClipRect(QRect(0, listTop(), width(), height() - listTop()));
	int y = listTop() - scroll_;
	for (int i = 0; i < rows.size(); ++i) {
		const auto &item = rows[i];
		const QRect row(0, y, width() - 1, theme.dialogsRowHeight);
		y += theme.dialogsRowHeight;
		if (row.bottom() < listTop() || row.top() > height()) {
			continue;
		}
		const bool active = item.id == selected_;
		const bool hover = i == hover_ && !active;
		if (active) {
			painter.fillRect(row, theme.dialogsBgActive);
		} else if (hover) {
			painter.fillRect(row, theme.dialogsBgOver);
		}

		const int photo = theme.dialogsPhoto;
		const QRect avatarRect(13, row.y() + (row.height() - photo) / 2, photo, photo);
		painter.drawPixmap(avatarRect.topLeft(), makeAvatar(item.id, item.name, photo, active));

		const auto nameColor = active ? theme.dialogsNameFgActive : theme.dialogsNameFg;
		const auto textColor = active ? theme.dialogsTextFgActive : theme.dialogsTextFg;
		const auto dateColor = active ? theme.dialogsDateFgActive : theme.dialogsDateFg;

		const auto time = formatDialogTime(item.lastTime);
		painter.setFont(theme.fontDate);
		const int timeW = painter.fontMetrics().horizontalAdvance(time);
		const int rightPad = 14;
		if (!time.isEmpty()) {
			painter.setPen(dateColor);
			painter.drawText(
				QRect(row.right() - rightPad - timeW, row.y() + 10, timeW, 18),
				Qt::AlignRight | Qt::AlignVCenter,
				time);
		}

		int badgeW = 0;
		if (item.unread > 0) {
			const auto badge = item.unread > 99 ? QStringLiteral("99+") : QString::number(item.unread);
			painter.setFont(theme.fontBadge);
			const int textW = painter.fontMetrics().horizontalAdvance(badge);
			badgeW = qMax(20, textW + 12);
			const QRect badgeRect(
				row.right() - rightPad - badgeW,
				row.y() + 38,
				badgeW,
				18);
			painter.setPen(Qt::NoPen);
			painter.setBrush(theme.dialogsUnreadBg);
			if (active) {
				painter.setBrush(QColor("#ffffff"));
			}
			painter.drawRoundedRect(badgeRect, 9, 9);
			painter.setPen(active ? theme.dialogsBgActive : theme.dialogsUnreadFg);
			painter.drawText(badgeRect, Qt::AlignCenter, badge);
		}

		const int textLeft = avatarRect.right() + 12;
		const int textRight = row.right() - rightPad - qMax(timeW, badgeW) - 8;
		painter.setFont(theme.fontName);
		painter.setPen(nameColor);
		QString name = item.name;
		if (item.isGroup && !name.endsWith(QLatin1Char(' '))) {
			// Name only; a small group mark is not required.
		}
		painter.drawText(
			QRect(textLeft, row.y() + 10, qMax(20, textRight - textLeft), 22),
			Qt::AlignLeft | Qt::AlignVCenter,
			painter.fontMetrics().elidedText(name, Qt::ElideRight, qMax(20, textRight - textLeft)));

		painter.setFont(theme.fontPreview);
		painter.setPen(textColor);
		auto preview = dialogPreviewText(item);
		const int previewRight = row.right() - rightPad - (item.unread > 0 ? badgeW + 8 : 0);
		painter.drawText(
			QRect(textLeft, row.y() + 36, qMax(20, previewRight - textLeft), 20),
			Qt::AlignLeft | Qt::AlignVCenter,
			painter.fontMetrics().elidedText(preview, Qt::ElideRight, qMax(20, previewRight - textLeft)));
	}
}

void DialogList::mousePressEvent(QMouseEvent *event) {
	if (QRect(8, 10, 36, 36).contains(event->pos())) {
		emit menuRequested();
		return;
	}
	const int index = rowAt(event->pos());
	if (index >= 0) {
		const auto rows = filtered();
		selected_ = rows[index].id;
		update();
		search_->clearFocus();
		emit chatSelected(selected_);
	}
}

void DialogList::mouseMoveEvent(QMouseEvent *event) {
	const int index = rowAt(event->pos());
	if (index != hover_) {
		hover_ = index;
		update();
	}
}

void DialogList::leaveEvent(QEvent *) {
	hover_ = -1;
	update();
}

void DialogList::wheelEvent(QWheelEvent *event) {
	const int delta = event->angleDelta().y();
	scroll_ -= delta / 2;
	updateScroll();
	update();
}
