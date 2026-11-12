#include "icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QHash>

QPixmap loadTintedIcon(const QString &resource, const QColor &color, int size) {
	static QHash<QString, QPixmap> cache;
	const QString key = resource
		+ QChar('#')
		+ QString::number(color.rgba(), 16)
		+ QChar('x')
		+ QString::number(size);
	if (const auto it = cache.constFind(key); it != cache.cend()) {
		return it.value();
	}
	QImage source(resource);
	if (source.isNull()) {
		QPixmap empty(size, size);
		empty.fill(Qt::transparent);
		return empty;
	}
	source = source.convertToFormat(QImage::Format_ARGB32);
	for (int y = 0; y < source.height(); ++y) {
		auto *line = reinterpret_cast<QRgb *>(source.scanLine(y));
		for (int x = 0; x < source.width(); ++x) {
			const auto pixel = line[x];
			const int r = qRed(pixel);
			const int g = qGreen(pixel);
			const int b = qBlue(pixel);
			const int a = qAlpha(pixel);
			const int luminance = (r * 30 + g * 59 + b * 11) / 100;
			// Official compose/search assets are light-on-black. Use
			// luminance as coverage so the glyph takes the tint color.
			const int coverage = (luminance * a) / 255;
			line[x] = qRgba(color.red(), color.green(), color.blue(), coverage);
		}
	}
	const auto pixmap = QPixmap::fromImage(source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	cache.insert(key, pixmap);
	return pixmap;
}

QIcon loadTintedQIcon(const QString &resource, const QColor &color, int size) {
	return QIcon(loadTintedIcon(resource, color, size));
}

void paintHamburger(QPainter *painter, const QRect &rect, const QColor &color) {
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	QPen pen(color, 2.0);
	pen.setCapStyle(Qt::RoundCap);
	painter->setPen(pen);
	const int cx = rect.center().x();
	const int cy = rect.center().y();
	const int w = 14;
	painter->drawLine(cx - w / 2, cy - 5, cx + w / 2, cy - 5);
	painter->drawLine(cx - w / 2, cy, cx + w / 2, cy);
	painter->drawLine(cx - w / 2, cy + 5, cx + w / 2, cy + 5);
	painter->restore();
}

void paintPlane(QPainter *painter, const QRect &rect, const QColor &color) {
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	const qreal x = rect.x();
	const qreal y = rect.y();
	const qreal w = rect.width();
	const qreal h = rect.height();
	QPainterPath path;
	path.moveTo(x + w * 0.08, y + h * 0.52);
	path.lineTo(x + w * 0.92, y + h * 0.18);
	path.lineTo(x + w * 0.48, y + h * 0.58);
	path.closeSubpath();
	QPainterPath wing;
	wing.moveTo(x + w * 0.08, y + h * 0.52);
	wing.lineTo(x + w * 0.42, y + h * 0.58);
	wing.lineTo(x + w * 0.32, y + h * 0.84);
	wing.closeSubpath();
	painter->setPen(Qt::NoPen);
	painter->setBrush(color);
	painter->drawPath(path);
	painter->drawPath(wing);
	painter->restore();
}
