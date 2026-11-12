#include "emoji_catalog.h"

#include <QHash>
#include <QVector>
#include <QFontMetrics>
#include <QPainter>
#include <QColor>
#include <QTextLayout>

namespace emoji {
namespace {

constexpr int kCols = 32;
constexpr int kRows = 16;
constexpr int kCell = 72;

QVector<QPixmap> g_sheets;
QHash<QString, const StickerItem *> g_stickers;
QHash<QString, int> g_indexByUtf8;
bool g_ready = false;

void ensureLoaded() {
	if (g_ready) {
		return;
	}
	g_sheets.resize(8);
	for (int i = 0; i < 8; ++i) {
		g_sheets[i] = QPixmap(QStringLiteral(":/emoji/emoji_%1.png").arg(i + 1));
	}
	for (int i = 0; i < kStickerCount; ++i) {
		g_stickers.insert(QString::fromUtf8(kStickers[i].id), &kStickers[i]);
	}
	for (int i = 0; i < kEmojiCount; ++i) {
		const auto text = QString::fromUtf8(kEmoji[i].utf8);
		g_indexByUtf8.insert(text, kEmoji[i].index);
		QString stripped = text;
		stripped.remove(QChar(0xFE0F));
		stripped.remove(QChar(0xFE0E));
		if (stripped != text && !g_indexByUtf8.contains(stripped)) {
			g_indexByUtf8.insert(stripped, kEmoji[i].index);
		}
	}
	g_ready = true;
}

} // namespace

QPixmap sprite(int index, int size) {
	ensureLoaded();
	if (index < 0) {
		return {};
	}
	const int sheet = index / (kCols * kRows);
	if (sheet < 0 || sheet >= g_sheets.size() || g_sheets[sheet].isNull()) {
		return {};
	}
	const int rem = index % (kCols * kRows);
	const int row = rem / kCols;
	const int col = rem % kCols;
	const QPixmap cell = g_sheets[sheet].copy(col * kCell, row * kCell, kCell, kCell);
	if (size <= 0 || size == kCell) {
		return cell;
	}
	return cell.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap stickerPixmap(const QString &id, int size) {
	const auto path = stickerResource(id);
	QPixmap pix(path);
	if (pix.isNull()) {
		if (const auto *item = findSticker(id)) {
			pix = sprite(item->index, size > 0 ? size : 128);
		}
	} else if (size > 0 && (pix.width() != size || pix.height() != size)) {
		pix = pix.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	return pix;
}

const StickerItem *findSticker(const QString &id) {
	ensureLoaded();
	return g_stickers.value(id, nullptr);
}

QString stickerResource(const QString &id) {
	return QStringLiteral(":/stickers/classic/%1.png").arg(id);
}

bool isStickerPayload(const QString &text) {
	return text.startsWith(QString::fromUtf8(kStickerPrefix));
}

QString stickerIdFromPayload(const QString &text) {
	if (!isStickerPayload(text)) {
		return {};
	}
	return text.mid(int(sizeof(kStickerPrefix) - 1));
}

QString payloadForSticker(const QString &id) {
	return QString::fromUtf8(kStickerPrefix) + id;
}

int indexOf(const QString &text) {
	ensureLoaded();
	if (text.isEmpty()) {
		return -1;
	}
	auto it = g_indexByUtf8.find(text);
	if (it != g_indexByUtf8.end()) {
		return it.value();
	}
	QString stripped = text;
	stripped.remove(QChar(0xFE0F));
	stripped.remove(QChar(0xFE0E));
	it = g_indexByUtf8.find(stripped);
	if (it != g_indexByUtf8.end()) {
		return it.value();
	}
	return -1;
}

namespace {

bool isSkinTone(uint cp) {
	return cp >= 0x1F3FB && cp <= 0x1F3FF;
}

// Tight ranges only. Do not use "cp >= 0x200D" — that also
// matches CJK ideographs and would paint them as empty chips.
bool isEmojiCodepoint(uint cp) {
	if (cp == 0x00A9 || cp == 0x00AE || cp == 0x203C || cp == 0x2049) {
		return true;
	}
	if (cp == 0x2122 || cp == 0x2139) {
		return true;
	}
	if ((cp >= 0x2190 && cp <= 0x21FF) || (cp >= 0x2300 && cp <= 0x23FF)) {
		return true;
	}
	if ((cp >= 0x25A0 && cp <= 0x25FF) || (cp >= 0x2600 && cp <= 0x27BF)) {
		return true;
	}
	if (cp == 0x2934 || cp == 0x2935 || cp == 0x2B05 || cp == 0x2B06 || cp == 0x2B07
		|| cp == 0x2B1B || cp == 0x2B1C || cp == 0x2B50 || cp == 0x2B55) {
		return true;
	}
	if (cp == 0x3030 || cp == 0x303D || cp == 0x3297 || cp == 0x3299) {
		return true;
	}
	if (cp >= 0x1F000 && cp <= 0x1FAFF) {
		return true;
	}
	return false;
}

bool isKeycapBase(uint cp) {
	return cp == 0x0023 || cp == 0x002A || (cp >= 0x0030 && cp <= 0x0039);
}

int codepointLength(const QString &text, int pos) {
	if (pos < 0 || pos >= text.size()) {
		return 0;
	}
	return (text[pos].isHighSurrogate() && pos + 1 < text.size() && text[pos + 1].isLowSurrogate()) ? 2 : 1;
}

uint codepointAt(const QString &text, int pos) {
	if (pos < 0 || pos >= text.size()) {
		return 0;
	}
	if (text[pos].isHighSurrogate() && pos + 1 < text.size() && text[pos + 1].isLowSurrogate()) {
		return QChar::surrogateToUcs4(text[pos], text[pos + 1]);
	}
	return text[pos].unicode();
}

int longestEmojiAt(const QString &text, int pos) {
	ensureLoaded();
	const int remaining = text.size() - pos;
	if (remaining <= 0) {
		return -1;
	}
	const int maxUnits = qMin(remaining, 16);
	int found = -1;
	for (int len = maxUnits; len > 0; --len) {
		// Do not split a surrogate pair.
		if (len >= 2 && text[pos + len - 1].isHighSurrogate()) {
			continue;
		}
		const auto slice = text.mid(pos, len);
		if (indexOf(slice) >= 0) {
			found = len;
			break;
		}
	}
	if (found > 0) {
		return found;
	}
	const int first = codepointLength(text, pos);
	if (first <= 0) {
		return -1;
	}
	const uint cp = codepointAt(text, pos);
	const bool keycap = isKeycapBase(cp) && pos + first < text.size()
		&& (codepointAt(text, pos + first) == 0xFE0F || codepointAt(text, pos + first) == 0x20E3);
	const bool maybeEmoji = isEmojiCodepoint(cp) || keycap;
	if (!maybeEmoji) {
		return -1;
	}
	int end = pos + first;
	while (end < text.size()) {
		const uint next = codepointAt(text, end);
		const int nlen = codepointLength(text, end);
		if (next == 0xFE0F || next == 0xFE0E || next == 0x200D || isSkinTone(next) || next == 0x20E3) {
			end += nlen;
			// After ZWJ, include the next pictograph as well.
			if (next == 0x200D && end < text.size()) {
				end += codepointLength(text, end);
			}
			continue;
		}
		// Flag sequences: regional indicator pairs.
		if (cp >= 0x1F1E6 && cp <= 0x1F1FF && next >= 0x1F1E6 && next <= 0x1F1FF && end == pos + first) {
			end += nlen;
			continue;
		}
		break;
	}
	if (indexOf(text.mid(pos, end - pos)) >= 0 || indexOf(text.mid(pos, first)) >= 0) {
		return end - pos;
	}
	// Known pictograph missing from the catalog: consume the cluster so
	// mixed text does not fall back to a missing system glyph.
	return end - pos;
}

int layoutEmojiText(
		const QFont &font,
		const QString &text,
		int width,
		int emojiSize,
		QVector<QVector<TextSpan>> *linesOut,
		QVector<int> *lineWidths) {
	const auto spans = tokenize(text);
	QFontMetrics fm(font);
	const int lineH = qMax(fm.height(), emojiSize);
	QVector<TextSpan> current;
	int currentW = 0;
	int height = 0;
	auto flush = [&]() {
		if (linesOut) {
			linesOut->push_back(current);
		}
		if (lineWidths) {
			lineWidths->push_back(currentW);
		}
		height += lineH;
		current.clear();
		currentW = 0;
	};
	for (const auto &span : spans) {
		if (span.sprite >= 0 || span.sprite == -2) {
			const int w = emojiSize;
			if (width > 0 && currentW > 0 && currentW + w > width) {
				flush();
			}
			TextSpan item = span;
			if (item.sprite < 0) {
				item.sprite = -2; // unknown emoji placeholder
			}
			current.push_back(item);
			currentW += w;
			continue;
		}
		int i = 0;
		while (i < span.text.size()) {
			const int n = codepointLength(span.text, i);
			const auto ch = span.text.mid(i, n);
			const int cw = fm.horizontalAdvance(ch);
			if (width > 0 && currentW > 0 && currentW + cw > width) {
				flush();
			}
			if (!current.isEmpty() && current.back().sprite < 0 && current.back().sprite != -2) {
				current.back().text += ch;
			} else {
				TextSpan piece;
				piece.text = ch;
				current.push_back(piece);
			}
			currentW += cw;
			i += n;
		}
	}
	if (!current.isEmpty() || height == 0) {
		flush();
	}
	return qMax(height, lineH);
}

} // namespace

QVector<TextSpan> tokenize(const QString &text) {
	QVector<TextSpan> out;
	int pos = 0;
	QString run;
	auto flush = [&]() {
		if (!run.isEmpty()) {
			TextSpan span;
			span.text = run;
			out.push_back(span);
			run.clear();
		}
	};
	while (pos < text.size()) {
		const int consumed = longestEmojiAt(text, pos);
		if (consumed > 0) {
			flush();
			TextSpan span;
			span.text = text.mid(pos, consumed);
			span.sprite = indexOf(span.text);
			if (span.sprite < 0) {
				// Try without modifiers.
				QString stripped = span.text;
				stripped.remove(QChar(0xFE0F));
				stripped.remove(QChar(0xFE0E));
				span.sprite = indexOf(stripped);
			}
			if (span.sprite < 0) {
				span.sprite = -2;
			}
			out.push_back(span);
			pos += consumed;
			continue;
		}
		const int n = codepointLength(text, pos);
		run += text.mid(pos, n);
		pos += n;
	}
	flush();
	return out;
}

int measureEmojiTextHeight(const QFont &font, const QString &text, int width, int emojiSize) {
	return layoutEmojiText(font, text, width, emojiSize, nullptr, nullptr);
}

int measureEmojiTextWidth(const QFont &font, const QString &text, int emojiSize) {
	QFontMetrics fm(font);
	int width = 0;
	for (const auto &span : tokenize(text)) {
		if (span.sprite != -1) {
			width += emojiSize;
		} else {
			width += fm.horizontalAdvance(span.text);
		}
	}
	return width;
}

void drawEmojiText(
		QPainter *painter,
		const QRect &rect,
		const QFont &font,
		const QColor &color,
		const QString &text,
		int emojiSize) {
	if (!painter) {
		return;
	}
	QVector<QVector<TextSpan>> lines;
	layoutEmojiText(font, text, rect.width(), emojiSize, &lines, nullptr);
	QFontMetrics fm(font);
	const int lineH = qMax(fm.height(), emojiSize);
	painter->setFont(font);
	painter->setPen(color);
	int y = rect.top();
	for (const auto &line : lines) {
		int x = rect.left();
		for (const auto &span : line) {
			if (span.sprite >= 0) {
				const auto pix = sprite(span.sprite, emojiSize);
				if (!pix.isNull()) {
					const int iy = y + (lineH - pix.height()) / 2;
					painter->drawPixmap(x, iy, pix);
					x += emojiSize;
					continue;
				}
			}
			if (span.sprite == -2) {
				// Official-style fallback chip, not a tofu box.
				QRect chip(x + 1, y + (lineH - emojiSize) / 2 + 1, emojiSize - 2, emojiSize - 2);
				painter->setPen(Qt::NoPen);
				painter->setBrush(QColor(color.red(), color.green(), color.blue(), 40));
				painter->drawRoundedRect(chip, 4, 4);
				painter->setPen(color);
				x += emojiSize;
				continue;
			}
			painter->setPen(color);
			painter->drawText(QRect(x, y, rect.right() - x + 1, lineH), Qt::AlignVCenter | Qt::AlignLeft, span.text);
			x += fm.horizontalAdvance(span.text);
		}
		y += lineH;
		if (y > rect.bottom()) {
			break;
		}
	}
}

} // namespace emoji
