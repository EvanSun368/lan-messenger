#include "avatar.h"
#include "theme.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>

namespace {
QHash<QString, QString> g_photos;
}

void setCachedAvatar(const QString &id, const QString &photoPath) {
	if (id.isEmpty()) {
		return;
	}
	if (photoPath.isEmpty()) {
		g_photos.remove(id);
	} else {
		g_photos.insert(id, photoPath);
	}
}

QString cachedAvatarPath(const QString &id) {
	return g_photos.value(id);
}

QPixmap makeAvatar(const QString &id, const QString &name, int size, bool selected, const QString &photoPath) {
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	const auto &theme = Theme::current();
	QPainterPath clip;
	clip.addEllipse(0, 0, size, size);
	painter.setClipPath(clip);

	const auto path = photoPath.isEmpty() ? cachedAvatarPath(id) : photoPath;
	if (!path.isEmpty()) {
		QPixmap src(path);
		if (!src.isNull()) {
			const auto scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
			const int x = (scaled.width() - size) / 2;
			const int y = (scaled.height() - size) / 2;
			painter.drawPixmap(0, 0, scaled, x, y, size, size);
			if (selected) {
				painter.setClipping(false);
				QPen pen(theme.windowFgActive, 2);
				painter.setPen(pen);
				painter.setBrush(Qt::NoBrush);
				painter.drawEllipse(1, 1, size - 2, size - 2);
			}
			return pixmap;
		}
	}

	const int index = Theme::userpicIndex(id.isEmpty() ? name : id);
	QColor bg = theme.userpic[index];
	if (selected) {
		bg = bg.lighter(110);
	}
	painter.setPen(Qt::NoPen);
	painter.setBrush(bg);
	painter.drawEllipse(0, 0, size, size);
	QFont font = theme.fontUserpic;
	font.setPixelSize(qMax(12, int(size * 0.42)));
	painter.setFont(font);
	painter.setPen(theme.userpicFg);
	painter.setClipping(false);
	painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, Theme::initialLetter(name));
	return pixmap;
}
