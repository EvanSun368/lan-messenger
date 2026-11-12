#pragma once

#include <QPixmap>
#include <QString>
#include <QVector>
#include <QRect>
#include <cstdint>

namespace emoji {

struct EmojiItem {
	uint8_t section;
	uint16_t index;
	const char *utf8;
};

struct StickerItem {
	const char *id;
	const char *utf8;
	uint16_t index;
};

extern const EmojiItem kEmoji[];
extern const int kEmojiCount;
extern const StickerItem kStickers[];
extern const int kStickerCount;

QPixmap sprite(int index, int size);
QPixmap stickerPixmap(const QString &id, int size);
int indexOf(const QString &text);

struct TextSpan {
	QString text;
	int sprite = -1;
};
QVector<TextSpan> tokenize(const QString &text);
int measureEmojiTextHeight(const QFont &font, const QString &text, int width, int emojiSize);
int measureEmojiTextWidth(const QFont &font, const QString &text, int emojiSize);
void drawEmojiText(class QPainter *painter, const QRect &rect, const QFont &font,
	const class QColor &color, const QString &text, int emojiSize);
const StickerItem *findSticker(const QString &id);
QString stickerResource(const QString &id);
// Messages use this prefix so a sticker can travel as sealed text.
inline constexpr const char kStickerPrefix[] = "llsticker:classic:";

bool isStickerPayload(const QString &text);
QString stickerIdFromPayload(const QString &text);
QString payloadForSticker(const QString &id);

} // namespace emoji
