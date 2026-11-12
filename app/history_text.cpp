#include "history_text.h"

#include <QPainter>
#include <QTextCharFormat>
#include <QTextOption>
#include <QtMath>

namespace {

QString layoutText(const QString &text) {
	QString out = text;
	out.replace(QChar(QChar::CarriageReturn), QChar(QChar::LineSeparator));
	out.replace(QChar(QChar::LineFeed), QChar(QChar::LineSeparator));
	return out;
}

int collapsedPrefix(int total, int width, int maxLines) {
	const int perLine = qMax(32, width / 2 + 16);
	return qMin(total, maxLines * perLine + 64);
}

QList<QTextLayout::FormatRange> markdownFormats(
		const markdown::ParsedText &parsed,
		int textStart,
		int textLength,
		const QFont &font,
		const QFont &mono,
		const QColor &color,
		const QColor &linkColor,
		const QColor &codeBg,
		bool dark,
		const QSet<QString> &revealed,
		const QString &msgId) {
	QList<QTextLayout::FormatRange> ranges;
	if (textLength <= 0) {
		return ranges;
	}
	const int textEnd = textStart + textLength;
	for (const auto &e : parsed.entities) {
		if (e.offset >= textEnd || e.offset + e.length <= textStart) {
			continue;
		}
		QTextLayout::FormatRange range;
		range.start = qMax(e.offset, textStart) - textStart;
		const int till = qMin(e.offset + e.length, textEnd) - textStart;
		range.length = till - range.start;
		if (range.length <= 0) {
			continue;
		}
		QTextCharFormat fmt;
		fmt.setForeground(color);
		if (e.type == markdown::Type::Bold) {
			QFont bold = font;
			bold.setWeight(QFont::DemiBold);
			fmt.setFont(bold);
			fmt.setFontWeight(QFont::DemiBold);
		} else if (e.type == markdown::Type::Italic) {
			fmt.setFontItalic(true);
		} else if (e.type == markdown::Type::Strike) {
			fmt.setFontStrikeOut(true);
		} else if (e.type == markdown::Type::Code || e.type == markdown::Type::Pre) {
			fmt.setFont(mono);
			fmt.setBackground(codeBg);
		} else if (e.type == markdown::Type::Link || e.type == markdown::Type::Url) {
			fmt.setForeground(linkColor);
			fmt.setFontUnderline(true);
			fmt.setAnchor(true);
			fmt.setAnchorHref(e.data);
		} else if (e.type == markdown::Type::Spoiler) {
			const auto key = msgId + QChar(':') + QString::number(e.offset);
			if (!revealed.contains(key)) {
				const QColor hide = dark ? QColor("#708499") : QColor("#c0c6cc");
				fmt.setForeground(hide);
				fmt.setBackground(hide);
			}
		}
		range.format = fmt;
		ranges.push_back(range);
	}
	return ranges;
}

} // namespace

void HistoryText::setSource(const QString &source) {
	if (source_ == source) {
		return;
	}
	source_ = source;
	parsed_ = markdown::parse(source_);
	prepared_ = layoutText(parsed_.entities.isEmpty() ? source_ : parsed_.text);
	invalidatePaint();
}

void HistoryText::invalidatePaint() {
	laidOut_ = false;
	laidOutTextLength_ = 0;
	lines_.clear();
	clearPaintWindow();
}

void HistoryText::clearPaintWindow() const {
	paintLayout_.clearLayout();
	paintFrom_ = -1;
	paintTill_ = -1;
	paintLineWindow_ = 0;
}

int HistoryText::lineCount() const {
	return laidOut_ ? lines_.size() : 0;
}

int HistoryText::firstLineAt(int y) const {
	int lo = 0;
	int hi = lines_.size();
	while (lo < hi) {
		const int mid = lo + (hi - lo) / 2;
		if (lines_[mid].y + lines_[mid].height <= y) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

int HistoryText::lastLineBefore(int y) const {
	int lo = 0;
	int hi = lines_.size();
	while (lo < hi) {
		const int mid = lo + (hi - lo) / 2;
		if (lines_[mid].y < y) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

HistoryText::Metrics HistoryText::layout(
		int width,
		bool expanded,
		int maxLines,
		const QFont &font,
		const QFont &mono,
		const QColor &color,
		const QColor &linkColor,
		const QColor &codeBg,
		bool dark,
		const QSet<QString> &revealedSpoilers,
		const QString &msgId) {
	width = qMax(1, width);
	maxLines = qMax(1, maxLines);
	if (laidOut_
		&& width_ == width
		&& expanded_ == expanded
		&& maxLines_ == maxLines
		&& color_ == color
		&& linkColor_ == linkColor
		&& codeBg_ == codeBg
		&& dark_ == dark
		&& msgId_ == msgId
		&& revealed_ == revealedSpoilers
		&& font_ == font
		&& mono_ == mono) {
		return metrics_;
	}
	rebuildLayout(
		width, expanded, maxLines, font, mono, color, linkColor, codeBg,
		dark, revealedSpoilers, msgId);
	return metrics_;
}

void HistoryText::rebuildLayout(
		int width,
		bool expanded,
		int maxLines,
		const QFont &font,
		const QFont &mono,
		const QColor &color,
		const QColor &linkColor,
		const QColor &codeBg,
		bool dark,
		const QSet<QString> &revealedSpoilers,
		const QString &msgId) {
	const QFontMetrics fm(font);
	const int lineH = qMax(fm.lineSpacing(), 20);
	const int maxH = maxLines * lineH;
	metrics_ = {};
	metrics_.lineH = lineH;
	clearPaintWindow();
	lines_.clear();
	++generation_;

	QTextOption option;
	option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

	auto breakLines = [&](const QString &text, int stopAtHeight) {
		lines_.clear();
		QTextLayout tmp;
		tmp.setCacheEnabled(false);
		tmp.setText(text);
		tmp.setFont(font);
		tmp.setTextOption(option);
		tmp.setFormats(markdownFormats(
			parsed_, 0, text.size(), font, mono, color, linkColor, codeBg,
			dark, revealedSpoilers, msgId));
		tmp.beginLayout();
		int y = 0;
		int consumed = 0;
		for (;;) {
			QTextLine line = tmp.createLine();
			if (!line.isValid()) {
				break;
			}
			line.setLineWidth(width);
			line.setPosition(QPointF(0, y));
			const int h = qMax(lineH, int(qCeil(line.height())));
			LineRec rec;
			rec.start = line.textStart();
			rec.length = line.textLength();
			rec.y = y;
			rec.height = h;
			lines_.push_back(rec);
			y += h;
			consumed = rec.start + rec.length;
			if (stopAtHeight > 0 && y >= stopAtHeight) {
				break;
			}
		}
		tmp.endLayout();
		return consumed;
	};

	int cap = prepared_.size();
	if (!expanded) {
		cap = collapsedPrefix(prepared_.size(), width, maxLines);
	}
	int consumed = 0;
	for (;;) {
		const QString chunk = prepared_.left(cap);
		consumed = breakLines(chunk, expanded ? 0 : maxH);
		const int y = lines_.isEmpty() ? 0 : (lines_.last().y + lines_.last().height);
		if (expanded || y >= maxH || cap >= prepared_.size()) {
			laidOutTextLength_ = chunk.size();
			break;
		}
		// Official only needs the first 50 lines; grow the prefix instead
		// of itemizing the rest of a 1 MiB source in one jump.
		cap = qMin(prepared_.size(), qMax(cap * 2, cap + collapsedPrefix(prepared_.size(), width, maxLines)));
	}

	const int y = lines_.isEmpty() ? 0 : (lines_.last().y + lines_.last().height);
	const bool more = consumed < prepared_.size();
	metrics_.height = y > 0 ? y : lineH;
	metrics_.lineCount = lines_.size();
	metrics_.clipped = !expanded && more;
	metrics_.fullHeight = metrics_.clipped ? y + lineH : y;
	if (expanded) {
		metrics_.clipped = false;
		metrics_.fullHeight = metrics_.height;
		laidOutTextLength_ = prepared_.size();
	} else if (consumed > 0) {
		// Remember only what the 50-line budget actually used.
		laidOutTextLength_ = consumed;
	}

	width_ = width;
	expanded_ = expanded;
	maxLines_ = maxLines;
	color_ = color;
	linkColor_ = linkColor;
	codeBg_ = codeBg;
	dark_ = dark;
	msgId_ = msgId;
	revealed_ = revealedSpoilers;
	font_ = font;
	mono_ = mono;
	laidOut_ = true;
}

void HistoryText::ensurePaintWindow(int fromLine, int tillLine) const {
	if (fromLine < 0) {
		fromLine = 0;
	}
	if (tillLine > lines_.size()) {
		tillLine = lines_.size();
	}
	if (fromLine >= tillLine) {
		clearPaintWindow();
		return;
	}
	if (paintFrom_ == fromLine
		&& paintTill_ == tillLine
		&& paintLayout_.lineCount() == tillLine - fromLine) {
		return;
	}
	const int textFrom = lines_[fromLine].start;
	const int textTill = lines_[tillLine - 1].start + lines_[tillLine - 1].length;
	const QString slice = prepared_.mid(textFrom, qMax(0, textTill - textFrom));
	QTextOption option;
	option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	paintLayout_.clearLayout();
	paintLayout_.setText(slice);
	paintLayout_.setFont(font_);
	paintLayout_.setTextOption(option);
	paintLayout_.setFormats(markdownFormats(
		parsed_, textFrom, slice.size(), font_, mono_, color_, linkColor_,
		codeBg_, dark_, revealed_, msgId_));
	paintLayout_.setCacheEnabled(true);
	paintLayout_.beginLayout();
	for (int i = fromLine; i < tillLine; ++i) {
		QTextLine line = paintLayout_.createLine();
		if (!line.isValid()) {
			break;
		}
		line.setLineWidth(width_);
		line.setPosition(QPointF(0, lines_[i].y - lines_[fromLine].y));
	}
	paintLayout_.endLayout();
	paintFrom_ = fromLine;
	paintTill_ = tillLine;
	paintLineWindow_ = paintLayout_.lineCount();
}

void HistoryText::draw(
		QPainter *painter,
		const QRect &rect,
		const QColor &color,
		const QRect &clip) const {
	if (!laidOut_ || !painter || lines_.isEmpty()) {
		return;
	}
	QRect visible = rect;
	if (clip.isValid() && !clip.isEmpty()) {
		visible = rect.intersected(clip);
	} else if (painter->hasClipping()) {
		visible = rect.intersected(painter->clipBoundingRect().toAlignedRect());
	}
	if (visible.isEmpty()) {
		return;
	}
	const int y0 = visible.top() - rect.top();
	const int y1 = visible.bottom() + 1 - rect.top();
	const int from = firstLineAt(y0);
	const int till = lastLineBefore(y1);
	if (from >= till) {
		return;
	}
	ensurePaintWindow(from, till);

	painter->save();
	// Intersect so we do not replace the official viewport clip with the
	// full bubble (QPainter::setClipRect defaults to Qt::ReplaceClip).
	painter->setClipRect(visible, Qt::IntersectClip);
	painter->setPen(color);
	painter->setFont(font_);
	const QPointF pos(rect.left(), rect.top() + lines_[from].y);
	paintLayout_.draw(painter, pos, {}, QRectF(visible));
	painter->restore();
}

QVector<HistoryText::Hotspot> HistoryText::hotspots(
		const QPoint &origin,
		const QRect &clip) const {
	QVector<Hotspot> spots;
	if (!laidOut_ || lines_.isEmpty()) {
		return spots;
	}
	int from = 0;
	int till = lines_.size();
	if (clip.isValid() && !clip.isEmpty()) {
		from = firstLineAt(clip.top() - origin.y());
		till = lastLineBefore(clip.bottom() + 1 - origin.y());
		if (from >= till) {
			return spots;
		}
	}
	ensurePaintWindow(from, till);
	const int textFrom = lines_[from].start;
	const int textLength = paintLayout_.text().size();
	const int textEnd = textFrom + textLength;
	for (const auto &e : parsed_.entities) {
		if (e.type != markdown::Type::Link && e.type != markdown::Type::Url
			&& e.type != markdown::Type::Spoiler) {
			continue;
		}
		if (e.offset >= textEnd || e.offset + e.length <= textFrom) {
			continue;
		}
		Hotspot spot;
		if (e.type == markdown::Type::Spoiler) {
			spot.spoilerKey = msgId_ + QChar(':') + QString::number(e.offset);
		} else {
			spot.url = e.data;
		}
		QRect united;
		for (int i = 0; i < paintLayout_.lineCount(); ++i) {
			const auto line = paintLayout_.lineAt(i);
			const int start = textFrom + line.textStart();
			const int end = start + line.textLength();
			if (e.offset + e.length <= start || e.offset >= end) {
				continue;
			}
			const int localFrom = qMax(e.offset, start) - textFrom;
			const int localTill = qMin(e.offset + e.length, end) - textFrom;
			const qreal x1 = line.cursorToX(localFrom);
			const qreal x2 = line.cursorToX(localTill);
			const QRect r(
				origin.x() + int(qMin(x1, x2)),
				origin.y() + lines_[from].y + int(line.y()),
				int(qAbs(x2 - x1)),
				int(qCeil(line.height())));
			united = united.isEmpty() ? r : united.united(r);
		}
		if (!united.isEmpty()) {
			spot.rect = united;
			spots.push_back(spot);
		}
	}
	return spots;
}

HistoryText::Hotspot HistoryText::hitTest(const QPoint &origin, const QPoint &pos) const {
	if (!laidOut_ || lines_.isEmpty()) {
		return {};
	}
	const int localY = pos.y() - origin.y();
	const int localX = pos.x() - origin.x();
	const int index = firstLineAt(localY);
	if (index < 0 || index >= lines_.size()) {
		return {};
	}
	const auto &rec = lines_[index];
	if (localY < rec.y || localY >= rec.y + rec.height) {
		return {};
	}
	const auto spots = hotspots(
		origin,
		QRect(origin.x(), origin.y() + rec.y, qMax(1, width_), rec.height));
	for (const auto &spot : spots) {
		if (spot.rect.contains(pos)) {
			return spot;
		}
	}
	Q_UNUSED(localX);
	return {};
}
