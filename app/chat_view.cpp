#include "chat_view.h"
#include "file_bubble.h"

#include "avatar.h"
#include "compose_bar.h"
#include "emoji_catalog.h"
#include "emoji_panel.h"
#include "icons.h"
#include "theme.h"
#include "i18n.h"
#include "markdown.h"
#include "history_text.h"
#include "popup_menu.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QTextLayout>
#include <QTextCharFormat>
#include <QTextOption>
#include <QtMath>
#include <QUrl>
#include <QCache>
#include <QClipboard>
#include <QApplication>
#include <memory>

namespace {

QCache<QString, QPixmap> g_photos(32);

// Official paints every message through Ui::Text::String. The emoji
// sprite painter stays only for short, unclipped, non-markdown bubbles
// where a per-paint tokenize is still cheap.
constexpr int kEmojiPainterMaxChars = 256;

QString formatTime(qint64 ms) {
	if (ms <= 0) {
		return {};
	}
	return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("hh:mm"));
}

QString formatDateLabel(qint64 ms) {
	const auto date = QDateTime::fromMSecsSinceEpoch(ms).date();
	const auto today = QDate::currentDate();
	if (date == today) {
		return i18n::tr("today");
	}
	if (date == today.addDays(-1)) {
		return i18n::tr("yesterday");
	}
	return QLocale().toString(date, QStringLiteral("MMMM d"));
}




FileBubbleMetrics metricsForFile(
		const ChatMessage &msg,
		int maxInner,
		int timeW,
		const Theme &theme,
		int captionHeight) {
	FileBubbleMetrics metrics;
	metrics.maxInnerWidth = maxInner;
	metrics.incomingOffer = msg.pendingAccept && !msg.outgoing
		&& (msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder);
	metrics.verifiedComplete = msg.verified && msg.complete;
	metrics.captionHeight = captionHeight;
	metrics.timeWidth = timeW;
	const auto fm = QFontMetrics(theme.fontDate);
	metrics.acceptWidth = qMax(56, fm.horizontalAdvance(i18n::tr("receive")) + 20);
	metrics.declineWidth = qMax(56, fm.horizontalAdvance(i18n::tr("decline")) + 20);
	metrics.saveAsWidth = qMax(80, fm.horizontalAdvance(i18n::tr("choose_save_location")) + 8);
	return metrics;
}

void drawChecks(QPainter *painter, const QRect &rect, bool acked, const QColor &color) {
	painter->save();
	QPen pen(color, 1.6);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter->setPen(pen);
	const int x = rect.right() - 2;
	const int y = rect.center().y();
	painter->drawLine(x - 7, y, x - 4, y + 3);
	painter->drawLine(x - 4, y + 3, x, y - 3);
	if (acked) {
		painter->drawLine(x - 11, y, x - 8, y + 3);
		painter->drawLine(x - 8, y + 3, x - 4, y - 3);
	}
	painter->restore();
}

QDate dateOf(qint64 ms) {
	if (ms <= 0) {
		return {};
	}
	return QDateTime::fromMSecsSinceEpoch(ms).date();
}

} // namespace

MessageList::MessageList(QWidget *parent)
	: QWidget(parent) {
	setAttribute(Qt::WA_OpaquePaintEvent, false);
}

QString MessageList::textKey(const ChatMessage &msg) const {
	return msg.id.isEmpty() ? QStringLiteral("anon:%1").arg(quintptr(&msg)) : msg.id;
}

HistoryText &MessageList::textFor(const ChatMessage &msg) const {
	const auto key = textKey(msg);
	auto &slot = historyText_[key];
	if (!slot) {
		slot = std::make_shared<HistoryText>();
	}
	slot->setSource(msg.text);
	return *slot;
}

void MessageList::pruneTextCache() {
	QSet<QString> alive;
	alive.reserve(messages_.size());
	for (const auto &msg : messages_) {
		if (!msg.id.isEmpty()) {
			alive.insert(msg.id);
		}
	}
	for (auto it = historyText_.begin(); it != historyText_.end(); ) {
		if (it.key().startsWith(QStringLiteral("anon:")) || !alive.contains(it.key())) {
			it = historyText_.erase(it);
		} else {
			++it;
		}
	}
}

void MessageList::applyTheme() {
	for (auto &slot : historyText_) {
		if (slot) {
			slot->invalidatePaint();
		}
	}
	update();
}

void MessageList::setMessages(
		const QVector<ChatMessage> &messages,
		bool isGroup,
		const QString &selfId) {
	messages_ = messages;
	isGroup_ = isGroup;
	selfId_ = selfId;
	pruneTextCache();
	relayout();
	update();
}

int MessageList::maxBubbleWidth() const {
	return qBound(180, int(width() * 0.62), 420);
}

int MessageList::measureTextHeight(const QString &text, int width) const {
	const auto &theme = Theme::current();
	return emoji::measureEmojiTextHeight(theme.fontText, text, width, 20);
}

MessageList::TextMetrics MessageList::measureBubbleText(const ChatMessage &msg, int width) const {
	const auto &theme = Theme::current();
	const bool expanded = expandedIds_.contains(msg.id);
	const auto color = msg.outgoing ? theme.historyTextOutFg : theme.historyTextInFg;
	auto &cached = textFor(msg);
	const auto laid = cached.layout(
		width,
		expanded,
		theme.historyTextMaxLines,
		theme.fontText,
		theme.fontMono,
		color,
		theme.historyLinkFg,
		theme.historyCodeBg,
		theme.dark,
		revealedSpoilers_,
		msg.id);
	TextMetrics metrics;
	metrics.height = laid.height;
	metrics.fullHeight = laid.fullHeight;
	metrics.clipped = laid.clipped;
	metrics.lineH = laid.lineH;
	return metrics;
}

void MessageList::drawBubbleText(
		QPainter *painter,
		const QRect &rect,
		const ChatMessage &msg,
		const QColor &color,
		const QRect &clip) const {
	const auto &theme = Theme::current();
	const bool expanded = expandedIds_.contains(msg.id);
	auto &cached = textFor(msg);
	const auto metrics = cached.layout(
		rect.width(),
		expanded,
		theme.historyTextMaxLines,
		theme.fontText,
		theme.fontMono,
		color,
		theme.historyLinkFg,
		theme.historyCodeBg,
		theme.dark,
		revealedSpoilers_,
		msg.id);
	// Short unclipped messages without markdown keep the emoji painter.
	// Long text and markdown use the cached official-style layout and
	// only paint the visible clip.
	if (cached.parsed().entities.isEmpty()
		&& msg.text.size() <= kEmojiPainterMaxChars
		&& !metrics.clipped) {
		emoji::drawEmojiText(painter, rect, theme.fontText, color, msg.text, 20);
		return;
	}
	cached.draw(painter, rect, color, clip);
}

QPixmap MessageList::photoOf(const ChatMessage &msg, int maxW, int maxH) const {
	if (msg.path.isEmpty() || !is_image_name(msg.filename, msg.mime)) {
		return {};
	}
	const auto key = msg.path + QChar(':') + QString::number(maxW);
	if (auto *cached = g_photos.object(key)) {
		return *cached;
	}
	QPixmap source(msg.path);
	if (source.isNull()) {
		return {};
	}
	auto *scaled = new QPixmap(
		source.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	g_photos.insert(key, scaled);
	return *scaled;
}

void MessageList::resizeEvent(QResizeEvent *) {
	relayout();
}

void MessageList::relayout() {
	items_.clear();
	const auto &theme = Theme::current();
	const int marginX = 16;
	const int maxW = maxBubbleWidth();
	int y = 16;
	QDate lastDate;
	QString lastFrom;
	bool lastOutgoing = false;

	for (int i = 0; i < messages_.size(); ++i) {
		const auto &msg = messages_[i];
		const auto date = dateOf(msg.timestampMs);
		if (date.isValid() && date != lastDate) {
			LayoutItem pill;
			pill.type = LayoutItem::Type::Date;
			pill.y = y;
			pill.height = 28;
			pill.dateText = formatDateLabel(msg.timestampMs);
			items_.push_back(pill);
			y += pill.height + 8;
			lastDate = date;
			lastFrom.clear();
		}

		const bool tight = !lastFrom.isEmpty()
			&& lastFrom == msg.fromId
			&& lastOutgoing == msg.outgoing
			&& msg.kind != ChatMessage::Kind::Service;
		if (!tight) {
			y += (i == 0) ? 0 : 6;
		}
		lastFrom = msg.fromId;
		lastOutgoing = msg.outgoing;

		if (msg.kind == ChatMessage::Kind::Service
			|| msg.kind == ChatMessage::Kind::Deleted) {
			const auto label = msg.kind == ChatMessage::Kind::Deleted
				? (msg.text.isEmpty() ? i18n::tr("message_deleted") : i18n::localize(msg.text))
				: i18n::localize(msg.text);
			const QFontMetrics fm(theme.fontService);
			const int maxW = qMax(80, width() - 32);
			const int needed = fm.boundingRect(label).width() + 28;
			const int w = qBound(80, needed, maxW);
			const int textH = needed > maxW
				? fm.boundingRect(QRect(0, 0, w - 24, 400), Qt::TextWordWrap, label).height()
				: fm.height();
			LayoutItem item;
			item.type = LayoutItem::Type::Message;
			item.messageIndex = i;
			item.y = y;
			item.height = qMax(28, textH + 14);
			items_.push_back(item);
			y += item.height + 8;
			continue;
		}

		const bool outgoing = msg.outgoing;
		const bool showAvatar = isGroup_ && !outgoing;
		const int avatarSpace = showAvatar ? theme.avatarInGroup + 8 : 0;
		const int padX = 10;
		const int padY = 7;
		const int timeW = 44 + (outgoing ? 16 : 0);
		int innerW = maxW - padX * 2;
		int contentH = 0;
		int contentW = 80;
		int nameH = 0;
		if (isGroup_ && !outgoing && (lastFrom.isEmpty() || !tight)) {
			nameH = 18;
		}

		QPixmap photo;
		if (msg.kind == ChatMessage::Kind::Sticker) {
			const auto id = emoji::stickerIdFromPayload(msg.text);
			photo = emoji::stickerPixmap(id, 128);
			if (photo.isNull()) {
				contentW = 128;
				contentH = 128;
			}
		} else if ((msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder)
			&& is_image_name(msg.filename, msg.mime)
			&& (msg.complete || outgoing)) {
			photo = photoOf(msg, maxW - 8, 280);
		}
		if (!photo.isNull()) {
			contentW = photo.width();
			contentH = photo.height();
		} else if (msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder) {
			const int fileMaxW = qBound(180, int(width() * 0.78), 480);
			const int inner = qMax(80, fileMaxW - padX * 2);
			int captionH = 0;
			if (!msg.text.isEmpty()) {
				captionH = measureTextHeight(msg.text, inner);
			}
			const auto fileLayout = layout_file_bubble(
				metricsForFile(msg, inner, timeW, theme, captionH));
			contentW = fileLayout.width;
			contentH = fileLayout.height;
		} else {
			const int textW = qMin(maxW - padX * 2, 400);
			const auto fm = QFontMetrics(theme.fontText);
			if (msg.text.size() > kEmojiPainterMaxChars) {
				contentW = textW;
			} else {
				const int single = emoji::measureEmojiTextWidth(theme.fontText, msg.text, 20);
				contentW = qBound(40, single + timeW / 3, textW);
				if (single + timeW < textW) {
					contentW = qMin(textW, single + 8);
					if (single + timeW + 8 < textW) {
						contentW = single + timeW;
					}
				}
			}
			const auto metrics = measureBubbleText(msg, contentW);
			contentH = metrics.height;
			if (metrics.clipped || expandedIds_.contains(msg.id)) {
				contentH += 18;
			}
			contentH = qMax(contentH, qMax(int(fm.height()), 20));
		}

		const int bubbleCap = (msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder)
			? qBound(180, int(width() * 0.78), 480)
			: maxW;
		const int bubbleW = qBound(56, contentW + padX * 2, bubbleCap);
		const bool fileBubble = (msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder)
			&& photo.isNull();
		const int bubbleH = padY + nameH + contentH + padY + (fileBubble ? 0 : 16);
		LayoutItem item;
		item.type = LayoutItem::Type::Message;
		item.messageIndex = i;
		item.y = y;
		item.height = bubbleH + (tight ? 2 : 4);
		if (outgoing) {
			item.bubble = QRect(width() - marginX - bubbleW, y, bubbleW, bubbleH);
		} else {
			item.bubble = QRect(marginX + avatarSpace, y, bubbleW, bubbleH);
		}
		if (!photo.isNull()) {
			item.media = QRect(
				item.bubble.x() + 4,
				item.bubble.y() + padY + nameH,
				photo.width(),
				photo.height());
		}
		if (transfer_in_progress(msg) && (!msg.pendingAccept || msg.outgoing)) {
			item.cancel = QRect(item.bubble.right() - 26, item.bubble.y() + padY + nameH + 10, 18, 18);
		}
		if ((msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder)
			&& photo.isNull()) {
			const int ox = item.bubble.x() + padX;
			const int oy = item.bubble.y() + padY + nameH;
			int captionH = 0;
			if (!msg.text.isEmpty()) {
				captionH = measureTextHeight(msg.text, qMax(80, item.bubble.width() - padX * 2));
			}
			const auto fileLayout = layout_file_bubble(
				metricsForFile(msg, qMax(80, item.bubble.width() - padX * 2), timeW, theme, captionH));
			item.accept = fileLayout.accept.isEmpty() ? QRect() : fileLayout.accept.translated(ox, oy);
			item.decline = fileLayout.decline.isEmpty() ? QRect() : fileLayout.decline.translated(ox, oy);
			item.saveAs = fileLayout.saveAs.isEmpty() ? QRect() : fileLayout.saveAs.translated(ox, oy);
			item.time = fileLayout.time.translated(ox, oy);
		}
		if (msg.kind == ChatMessage::Kind::Text) {
			const int textW = qMax(1, item.bubble.width() - padX * 2);
			const auto metrics = measureBubbleText(msg, textW);
			item.textRect = QRect(
				item.bubble.x() + padX,
				item.bubble.y() + padY + nameH,
				textW,
				metrics.height);
			if (metrics.clipped || expandedIds_.contains(msg.id)) {
				// Keep a control when an oversized bubble was expanded
				// so the user can collapse it again (official "Show more").
				item.showMore = QRect(
					item.bubble.x() + padX,
					item.bubble.bottom() - 18 - 16,
					120,
					16);
			}
		}
		items_.push_back(item);
		y += item.height;
	}

	contentHeight_ = y + 16;
	const int viewH = parentWidget() ? parentWidget()->height() : 0;
	if (contentHeight_ < viewH) {
		const int pad = viewH - contentHeight_;
		for (auto &item : items_) {
			item.y += pad;
			item.bubble.translate(0, pad);
			item.media.translate(0, pad);
			item.cancel.translate(0, pad);
			item.showMore.translate(0, pad);
			item.accept.translate(0, pad);
			item.decline.translate(0, pad);
			item.saveAs.translate(0, pad);
			item.time.translate(0, pad);
			item.textRect.translate(0, pad);
		}
		contentHeight_ = viewH;
	}
	setFixedHeight(contentHeight_);
}

void MessageList::paintEvent(QPaintEvent *event) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	const QRect clip = event ? event->rect() : rect();
	painter.setRenderHint(QPainter::Antialiasing);

	for (const auto &item : items_) {
		const QRect bounds = item.bubble.isEmpty()
			? QRect(0, item.y, width(), item.height)
			: item.bubble.adjusted(-8, -4, 8, 8);
		if (!bounds.intersects(clip)) {
			continue;
		}
		if (item.type == LayoutItem::Type::Date) {
			const auto fm = QFontMetrics(theme.fontService);
			const int w = fm.horizontalAdvance(item.dateText) + 18;
			const QRect pill((width() - w) / 2, item.y + 4, w, 22);
			painter.setPen(Qt::NoPen);
			painter.setBrush(theme.msgServiceBg);
			painter.drawRoundedRect(pill, 11, 11);
			painter.setPen(theme.msgServiceFg);
			painter.setFont(theme.fontService);
			painter.drawText(pill, Qt::AlignCenter, item.dateText);
			continue;
		}
		if (item.messageIndex < 0 || item.messageIndex >= messages_.size()) {
			continue;
		}
		const auto &msg = messages_[item.messageIndex];
		if (msg.kind == ChatMessage::Kind::Service
			|| msg.kind == ChatMessage::Kind::Deleted) {
			const auto label = msg.kind == ChatMessage::Kind::Deleted
				? (msg.text.isEmpty() ? i18n::tr("message_deleted") : i18n::localize(msg.text))
				: i18n::localize(msg.text);
			painter.setFont(theme.fontService);
			const auto fm = painter.fontMetrics();
			const int maxW = qMax(80, width() - 32);
			const int needed = fm.boundingRect(label).width() + 28;
			const int w = qBound(80, needed, maxW);
			const int textH = needed > maxW
				? fm.boundingRect(QRect(0, 0, w - 24, 400), Qt::TextWordWrap, label).height()
				: fm.height();
			const int h = qMax(22, textH + 10);
			const QRect pill((width() - w) / 2, item.y + 4, w, h);
			painter.setPen(Qt::NoPen);
			painter.setBrush(theme.msgServiceBg);
			painter.drawRoundedRect(pill, 11, 11);
			painter.setPen(theme.msgServiceFg);
			painter.setFont(theme.fontService);
			painter.drawText(pill, Qt::AlignCenter | Qt::TextWordWrap, label);
			continue;
		}

		const bool outgoing = msg.outgoing;
		if (isGroup_ && !outgoing) {
			const QRect avatar(
				item.bubble.x() - Theme::current().avatarInGroup - 8,
				item.bubble.bottom() - Theme::current().avatarInGroup,
				Theme::current().avatarInGroup,
				Theme::current().avatarInGroup);
			if (avatar.x() >= 8) {
				painter.drawPixmap(
					avatar.topLeft(),
					makeAvatar(msg.fromId, msg.fromName, Theme::current().avatarInGroup));
			}
		}

		const auto bg = outgoing ? theme.msgOutBg : theme.msgInBg;
		const auto shadow = outgoing ? theme.msgOutShadow : theme.msgInShadow;
		QRect bubble = item.bubble;
		painter.setPen(Qt::NoPen);
		painter.setBrush(shadow);
		painter.drawRoundedRect(bubble.translated(0, 1), theme.bubbleRadius, theme.bubbleRadius);
		painter.setBrush(bg);
		painter.drawRoundedRect(bubble, theme.bubbleRadius, theme.bubbleRadius);

		// Tail at the bottom corner.
		QPainterPath tail;
		if (outgoing) {
			const int x = bubble.right();
			const int y = bubble.bottom();
			tail.moveTo(x - 6, y - 8);
			tail.lineTo(x + 5, y);
			tail.lineTo(x - 6, y);
			tail.closeSubpath();
		} else {
			const int x = bubble.left();
			const int y = bubble.bottom();
			tail.moveTo(x + 6, y - 8);
			tail.lineTo(x - 5, y);
			tail.lineTo(x + 6, y);
			tail.closeSubpath();
		}
		painter.fillPath(tail, bg);

		const int padX = 10;
		int cursorY = bubble.y() + 7;
		if (isGroup_ && !outgoing) {
			painter.setFont(theme.fontName);
			const int colorIndex = Theme::userpicIndex(msg.fromId);
			painter.setPen(theme.peerName[colorIndex]);
			painter.drawText(
				QRect(bubble.x() + padX, cursorY, bubble.width() - padX * 2, 16),
				Qt::AlignLeft | Qt::AlignVCenter,
				msg.fromName);
			cursorY += 18;
		}

		const auto textColor = outgoing ? theme.historyTextOutFg : theme.historyTextInFg;
		const auto dateColor = outgoing ? theme.msgOutDateFg : theme.msgInDateFg;

		if (!item.media.isEmpty()) {
			const auto photo = photoOf(msg, item.media.width(), item.media.height());
			if (!photo.isNull()) {
				QPainterPath clip;
				clip.addRoundedRect(item.media, 8, 8);
				painter.save();
				painter.setClipPath(clip);
				painter.drawPixmap(item.media.topLeft(), photo);
				painter.restore();
			}
		} else if (msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder) {
			const QRect icon(bubble.x() + padX, cursorY + 4, 40, 40);
			painter.setPen(Qt::NoPen);
			painter.setBrush(theme.historyFile1Bg);
			if (msg.kind == ChatMessage::Kind::Folder) {
				painter.setBrush(theme.historyFile4Bg);
			}
			painter.drawRoundedRect(icon, 8, 8);
			painter.setPen(Qt::white);
			painter.setFont(theme.fontBadge);
			painter.drawText(
				icon,
				Qt::AlignCenter,
				msg.kind == ChatMessage::Kind::Folder ? i18n::tr("dir") : i18n::tr("file").toUpper());
			painter.setPen(textColor);
			painter.setFont(theme.fontName);
			const QRect nameRect(icon.right() + 10, cursorY + 4, bubble.right() - padX - icon.right() - 10, 20);
			painter.drawText(
				nameRect,
				Qt::AlignLeft | Qt::AlignVCenter,
				painter.fontMetrics().elidedText(msg.filename, Qt::ElideMiddle, nameRect.width()));
			painter.setFont(theme.fontDate);
			painter.setPen(dateColor);
			painter.drawText(
				QRect(nameRect.x(), nameRect.bottom(), nameRect.width(), 18),
				Qt::AlignLeft | Qt::AlignVCenter,
				file_transfer_status(msg));
		} else {
			const int textBottom = item.showMore.isEmpty()
				? bubble.bottom() - 20
				: item.showMore.top() - 2;
			drawBubbleText(
				&painter,
				QRect(bubble.x() + padX, cursorY, bubble.width() - padX * 2, qMax(16, textBottom - cursorY)),
				msg,
				textColor,
				clip);
		}

		if (!item.cancel.isEmpty()) {
			const auto icon = loadTintedIcon(
				QStringLiteral(":/icons/file_cancel.png"),
				item.messageIndex == cancelHover_ ? theme.attentionFg : theme.composeIconFg,
				18);
			if (!icon.isNull()) {
				painter.drawPixmap(item.cancel.topLeft(), icon);
			} else {
				painter.setPen(QPen(theme.attentionFg, 2));
				painter.drawLine(item.cancel.topLeft() + QPoint(4, 4), item.cancel.bottomRight() - QPoint(4, 4));
				painter.drawLine(item.cancel.topRight() + QPoint(-4, 4), item.cancel.bottomLeft() + QPoint(4, -4));
			}
		}

		if (!item.showMore.isEmpty()) {
			painter.setFont(theme.fontDate);
			painter.setPen(theme.historyLinkFg);
			painter.drawText(
				item.showMore,
				Qt::AlignLeft | Qt::AlignVCenter,
				expandedIds_.contains(msg.id) ? i18n::tr("show_less") : i18n::tr("show_more"));
		}
		if (!item.accept.isEmpty()) {
			auto fillButton = [&](const QRect &rect, const QColor &bg, const QColor &fg, const QString &label, bool filled) {
				painter.setPen(filled ? Qt::NoPen : QPen(fg, 1));
				painter.setBrush(filled ? bg : Qt::NoBrush);
				painter.drawRoundedRect(rect, 6, 6);
				painter.setPen(fg);
				painter.setFont(theme.fontDate);
				painter.drawText(rect, Qt::AlignCenter, label);
			};
			fillButton(item.accept, theme.windowBgActive, QColor("#ffffff"), i18n::tr("receive"), true);
			fillButton(item.decline, QColor(0, 0, 0, 0), theme.attentionFg, i18n::tr("decline"), false);
			painter.setPen(theme.historyLinkFg);
			painter.setFont(theme.fontDate);
			painter.drawText(item.saveAs, Qt::AlignLeft | Qt::AlignVCenter, i18n::tr("choose_save_location"));
		}

		const QString time = formatTime(msg.timestampMs);
		const int timeBoxW = outgoing ? 52 : 36;
		const QRect timeRect = item.time.isEmpty()
			? QRect(bubble.right() - padX - timeBoxW, bubble.bottom() - 18, timeBoxW, 14)
			: (outgoing ? QRect(item.time.right() - timeBoxW, item.time.y(), timeBoxW, qMax(14, item.time.height())) : item.time);
		painter.setFont(theme.fontDate);
		painter.setPen(dateColor);
		painter.drawText(
			outgoing ? timeRect.adjusted(0, 0, -14, 0) : timeRect,
			Qt::AlignRight | Qt::AlignVCenter,
			time);
		if (outgoing) {
			const auto checkColor = msg.acked ? theme.historyOutIconFg : theme.historySendingOutIconFg;
			drawChecks(&painter, timeRect, msg.acked, checkColor);
		}
	}
}

int MessageList::itemAt(const QPoint &pos) const {
	for (int i = 0; i < items_.size(); ++i) {
		const auto &item = items_[i];
		if (item.messageIndex < 0) {
			continue;
		}
		if (item.bubble.contains(pos) || item.media.contains(pos) || item.cancel.contains(pos)
			|| item.showMore.contains(pos) || item.accept.contains(pos)
			|| item.decline.contains(pos) || item.saveAs.contains(pos)) {
			return i;
		}
	}
	return -1;
}

void MessageList::mouseMoveEvent(QMouseEvent *event) {
	int hover = -1;
	bool overLink = false;
	for (const auto &item : items_) {
		if (!item.cancel.isEmpty() && item.cancel.contains(event->pos())) {
			hover = item.messageIndex;
			break;
		}
		if (item.textRect.contains(event->pos())
			&& item.messageIndex >= 0
			&& item.messageIndex < messages_.size()) {
			const auto &msg = messages_[item.messageIndex];
			if (msg.kind == ChatMessage::Kind::Text) {
				const auto hit = textFor(msg).hitTest(item.textRect.topLeft(), event->pos());
				if (!hit.url.isEmpty() || !hit.spoilerKey.isEmpty()) {
					overLink = true;
				}
			}
		}
	}
	setCursor(overLink ? Qt::PointingHandCursor : Qt::ArrowCursor);
	if (hover != cancelHover_) {
		cancelHover_ = hover;
		update();
	}
}

void MessageList::mousePressEvent(QMouseEvent *event) {
	if (event->button() != Qt::LeftButton) {
		return;
	}
	const int index = itemAt(event->pos());
	if (index < 0) {
		return;
	}
	const auto &item = items_[index];
	const auto &msg = messages_[item.messageIndex];
	if (!item.cancel.isEmpty() && item.cancel.contains(event->pos())
		&& !msg.transferId.isEmpty()) {
		emit cancelTransfer(msg.transferId);
		return;
	}
	if (!item.showMore.isEmpty() && item.showMore.contains(event->pos()) && !msg.id.isEmpty()) {
		if (expandedIds_.contains(msg.id)) {
			expandedIds_.remove(msg.id);
		} else {
			expandedIds_.insert(msg.id);
		}
		relayout();
		update();
		return;
	}
	if (!item.accept.isEmpty() && item.accept.contains(event->pos()) && !msg.transferId.isEmpty()) {
		emit acceptTransfer(msg.transferId);
		return;
	}
	if (!item.decline.isEmpty() && item.decline.contains(event->pos()) && !msg.transferId.isEmpty()) {
		emit rejectTransfer(msg.transferId);
		return;
	}
	if (!item.saveAs.isEmpty() && item.saveAs.contains(event->pos()) && !msg.transferId.isEmpty()) {
		emit chooseSavePath(
			msg.transferId,
			msg.filename,
			msg.kind == ChatMessage::Kind::Folder);
		return;
	}
	if (!item.textRect.isEmpty() && item.textRect.contains(event->pos())) {
		const auto hit = textFor(msg).hitTest(item.textRect.topLeft(), event->pos());
		if (!hit.spoilerKey.isEmpty()) {
			if (revealedSpoilers_.contains(hit.spoilerKey)) {
				revealedSpoilers_.remove(hit.spoilerKey);
			} else {
				revealedSpoilers_.insert(hit.spoilerKey);
			}
			relayout();
			update();
			return;
		}
		if (!hit.url.isEmpty()) {
			QDesktopServices::openUrl(QUrl(hit.url));
			return;
		}
	}
	if ((msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder)
		&& msg.complete && !msg.path.isEmpty()) {
		emit openPath(msg.path);
	}
}

void MessageList::contextMenuEvent(QContextMenuEvent *event) {
	const int index = itemAt(event->pos());
	if (index < 0) {
		return;
	}
	const auto &msg = messages_[items_[index].messageIndex];
	QVector<PopupMenu::Item> items;
	QString text = msg.kind == ChatMessage::Kind::Sticker ? QString() : msg.text;
	if (!text.isEmpty() && msg.kind != ChatMessage::Kind::Deleted) {
		const auto parsed = markdown::parse(text);
		if (!parsed.entities.isEmpty()) {
			text = parsed.text;
		}
		PopupMenu::Item copy;
		copy.id = QStringLiteral("copy");
		copy.text = i18n::tr("copy_text");
		copy.icon = QStringLiteral(":/icons/menu_copy.png");
		copy.onClick = [text]() {
			if (auto *clip = QApplication::clipboard()) {
				clip->setText(text);
			}
		};
		items.push_back(copy);
	}
	if (msg.complete && !msg.path.isEmpty()
		&& (msg.kind == ChatMessage::Kind::File || msg.kind == ChatMessage::Kind::Folder)) {
		PopupMenu::Item open;
		open.id = QStringLiteral("open");
		open.text = i18n::tr("open");
		open.icon = QStringLiteral(":/icons/menu_file.png");
		open.onClick = [this, path = msg.path]() { emit openPath(path); };
		items.push_back(open);
	}
	if (msg.pendingAccept && !msg.outgoing && !msg.transferId.isEmpty()) {
		PopupMenu::Item accept;
		accept.id = QStringLiteral("receive");
		accept.text = i18n::tr("receive");
		accept.onClick = [this, id = msg.transferId]() { emit acceptTransfer(id); };
		items.push_back(accept);
		PopupMenu::Item decline;
		decline.id = QStringLiteral("decline");
		decline.text = i18n::tr("decline");
		decline.danger = true;
		decline.onClick = [this, id = msg.transferId]() { emit rejectTransfer(id); };
		items.push_back(decline);
		PopupMenu::Item saveAs;
		saveAs.id = QStringLiteral("saveas");
		saveAs.text = i18n::tr("choose_save_location");
		saveAs.onClick = [this, id = msg.transferId, name = msg.filename, folder = (msg.kind == ChatMessage::Kind::Folder)]() {
			emit chooseSavePath(id, name, folder);
		};
		items.push_back(saveAs);
	}
	if (transfer_in_progress(msg) && !msg.pendingAccept && !msg.transferId.isEmpty()) {
		PopupMenu::Item cancel;
		cancel.id = QStringLiteral("cancel");
		cancel.text = i18n::tr("cancel");
		cancel.icon = QStringLiteral(":/icons/file_cancel.png");
		cancel.danger = true;
		cancel.onClick = [this, id = msg.transferId]() { emit cancelTransfer(id); };
		items.push_back(cancel);
	}
	if (can_recall_message(msg, QDateTime::currentMSecsSinceEpoch())) {
		PopupMenu::Item recall;
		recall.id = QStringLiteral("recall");
		recall.text = i18n::tr("recall");
		recall.icon = QStringLiteral(":/icons/menu_delete.png");
		recall.danger = true;
		recall.onClick = [this, id = msg.id]() { emit recallMessage(id); };
		items.push_back(recall);
	}
	if (items.isEmpty()) {
		return;
	}
	auto *menu = new PopupMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);
	menu->setItems(items);
	menu->popup(event->globalPos());
}

ChatView::ChatView(QWidget *parent)
	: QWidget(parent) {
	list_ = new MessageList;
	scroll_ = new QScrollArea(this);
	scroll_->setWidget(list_);
	scroll_->setWidgetResizable(false);
	scroll_->setFrameShape(QFrame::NoFrame);
	scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll_->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
	scroll_->viewport()->setAutoFillBackground(false);
	list_->setAutoFillBackground(false);
	compose_ = new ComposeBar(this);
	emoji_ = new EmojiPanel(this);
	emoji_->hide();
	setAcceptDrops(true);
	connect(compose_, &ComposeBar::sendText, this, &ChatView::sendText);
	connect(compose_, &ComposeBar::attachFile, this, &ChatView::attachFile);
	connect(compose_, &ComposeBar::attachFolder, this, &ChatView::attachFolder);
	connect(compose_, &ComposeBar::attachPhoto, this, &ChatView::attachPhoto);
	connect(compose_, &ComposeBar::emojiToggled, this, &ChatView::toggleEmoji);
	connect(compose_, &ComposeBar::heightAdjusted, this, [this]() {
		layoutChat();
	});
	connect(emoji_, &EmojiPanel::emojiChosen, this, [this](const QString &text) {
		compose_->insertText(text);
		if (emoji_) {
			emoji_->hide();
		}
		compose_->focusEdit();
	});
	connect(emoji_, &EmojiPanel::stickerChosen, this, [this](const QString &id) {
		emit sendText(emoji::payloadForSticker(id));
		if (emoji_) {
			emoji_->hide();
		}
	});
	connect(list_, &MessageList::openPath, this, [](const QString &path) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	});
	connect(list_, &MessageList::cancelTransfer, this, &ChatView::cancelTransfer);
	connect(list_, &MessageList::recallMessage, this, &ChatView::recallMessage);
	connect(list_, &MessageList::acceptTransfer, this, &ChatView::acceptTransfer);
	connect(list_, &MessageList::rejectTransfer, this, &ChatView::rejectTransfer);
	connect(list_, &MessageList::chooseSavePath, this, &ChatView::chooseSavePath);
	connect(&Theme::notifier(), &ThemeNotifier::changed, this, &ChatView::applyTheme);
	connect(&i18n::notifier(), &LanguageNotifier::changed, this, [this]() {
		if (compose_) {
			compose_->applyTheme();
		}
		update();
	});
	compose_->hide();
}

void ChatView::applyTheme() {
	if (emoji_) {
		emoji_->applyTheme();
	}
	if (compose_) {
		compose_->applyTheme();
	}
	if (list_) {
		list_->applyTheme();
	}
	if (scroll_) {
		scroll_->viewport()->update();
	}
	update();
}

void ChatView::toggleEmoji() {
	if (!hasChat_ || !emoji_) {
		return;
	}
	if (emoji_->isVisible()) {
		emoji_->hide();
		return;
	}
	const QPoint global = mapToGlobal(QPoint(width() - emoji_->width() - 12, height() - compose_->height() - emoji_->height() - 4));
	emoji_->move(global);
	emoji_->show();
	emoji_->raise();
}

void ChatView::setSelf(const QString &id, const QString &name) {
	selfId_ = id;
	selfName_ = name;
}

void ChatView::setDialog(const Dialog &dialog, const QVector<ChatMessage> &messages) {
	const bool stick = !scroll_->verticalScrollBar()
		|| scroll_->verticalScrollBar()->value()
			>= scroll_->verticalScrollBar()->maximum() - 40;
	dialog_ = dialog;
	hasChat_ = !dialog.id.isEmpty();
	compose_->setEnabledChat(hasChat_);
	compose_->setVisible(hasChat_);
	layoutChat();
	list_->setFixedWidth(qMax(100, scroll_->viewport()->width()));
	list_->setMessages(messages, dialog.isGroup, selfId_);
	if (stick) {
		QTimer::singleShot(0, this, &ChatView::scrollToBottom);
	}
	update();
}

void ChatView::clearChat() {
	dialog_ = {};
	hasChat_ = false;
	compose_->setEnabledChat(false);
	compose_->hide();
	layoutChat();
	list_->setMessages({}, false, selfId_);
	update();
}

void ChatView::focusCompose() {
	if (hasChat_) {
		compose_->focusEdit();
	}
}

void ChatView::clearCompose() {
	if (compose_) {
		compose_->clearDraft();
	}
}

void ChatView::scrollToBottom() {
	scroll_->verticalScrollBar()->setValue(scroll_->verticalScrollBar()->maximum());
}

void ChatView::layoutChat() {
	const auto &theme = Theme::current();
	const int header = theme.topBarHeight;
	const int compose = reservedComposeHeight(hasChat_, compose_->height(), theme.composeMinHeight);
	if (hasChat_) {
		compose_->setGeometry(0, height() - compose, width(), compose);
		compose_->show();
		compose_->raise();
	} else {
		compose_->hide();
	}
	scroll_->setGeometry(0, header, width(), qMax(0, height() - header - compose));
	list_->setFixedWidth(scroll_->viewport()->width());
}

void ChatView::resizeEvent(QResizeEvent *) {
	layoutChat();
	if (emoji_ && emoji_->isVisible()) {
		toggleEmoji();
		toggleEmoji();
	}
}

QRect ChatView::membersHotspot() const {
	const auto &theme = Theme::current();
	if (!hasChat_ || !dialog_.isGroup) {
		return {};
	}
	return QRect(0, 0, width(), theme.topBarHeight);
}

void ChatView::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton && membersHotspot().contains(event->pos())) {
		emit editMembers();
		return;
	}
	if (event->button() == Qt::LeftButton && hasChat_ && event->pos().y() < Theme::current().topBarHeight
		&& event->pos().x() < width() - 110) {
		emit showPeerInfo();
		return;
	}
	QWidget::mousePressEvent(event);
}

bool ChatView::acceptDrop(const QMimeData *data) const {
	if (!hasChat_ || !data) {
		return false;
	}
	if (!data->hasUrls()) {
		return false;
	}
	for (const auto &url : data->urls()) {
		if (url.isLocalFile()) {
			return true;
		}
	}
	return false;
}

void ChatView::dragEnterEvent(QDragEnterEvent *event) {
	if (acceptDrop(event->mimeData())) {
		event->acceptProposedAction();
	} else {
		event->ignore();
	}
}

void ChatView::dragMoveEvent(QDragMoveEvent *event) {
	if (acceptDrop(event->mimeData())) {
		event->acceptProposedAction();
	} else {
		event->ignore();
	}
}

void ChatView::dropEvent(QDropEvent *event) {
	if (!acceptDrop(event->mimeData())) {
		event->ignore();
		return;
	}
	QStringList paths;
	for (const auto &url : event->mimeData()->urls()) {
		if (url.isLocalFile()) {
			paths.push_back(url.toLocalFile());
		}
	}
	if (!paths.isEmpty()) {
		emit sendPaths(paths);
		event->acceptProposedAction();
	}
}

void ChatView::paintEvent(QPaintEvent *) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const auto &wallpaper = Theme::wallpaper();
	if (!wallpaper.isNull()) {
		const int tileW = wallpaper.width();
		const int tileH = wallpaper.height();
		for (int y = theme.topBarHeight; y < height(); y += tileH) {
			for (int x = 0; x < width(); x += tileW) {
				painter.drawPixmap(x, y, wallpaper);
			}
		}
	} else {
		painter.fillRect(rect(), theme.dark ? QColor("#0e1621") : QColor("#c9d6dc"));
	}

	// Header.
	painter.fillRect(QRect(0, 0, width(), theme.topBarHeight), theme.topBarBg);
	painter.fillRect(QRect(0, theme.topBarHeight, width(), 1), theme.inputBorderFg);

	if (!hasChat_) {
		painter.setPen(theme.windowSubTextFg);
		painter.setFont(theme.fontPreview);
		painter.drawText(
			QRect(0, theme.topBarHeight, width(), height() - theme.topBarHeight),
			Qt::AlignCenter,
			i18n::tr("select_chat"));
		return;
	}

	const int photo = 42;
	const QRect avatar(16, (theme.topBarHeight - photo) / 2, photo, photo);
	painter.drawPixmap(avatar.topLeft(), makeAvatar(dialog_.id, dialog_.name, photo));
	painter.setFont(theme.fontHeader);
	painter.setPen(theme.windowBoldFg);
	painter.drawText(
		QRect(avatar.right() + 12, 8, width() - avatar.right() - 120, 22),
		Qt::AlignLeft | Qt::AlignVCenter,
		dialog_.name);
	painter.setFont(theme.fontStatus);
	QString status;
	if (dialog_.isGroup) {
		status = QString::number(dialog_.memberIds.size()) + QChar(' ') + i18n::tr("members");
	} else if (dialog_.online) {
		status = i18n::tr("on_lan");
		if (!dialog_.ip.isEmpty()) {
			status += QStringLiteral(" · ") + dialog_.ip;
		}
	} else {
		status = i18n::tr("offline");
	}
	painter.setPen(dialog_.online && !dialog_.isGroup ? theme.onlineFg : theme.windowSubTextFg);
	painter.drawText(
		QRect(avatar.right() + 12, 28, width() - avatar.right() - 120, 18),
		Qt::AlignLeft | Qt::AlignVCenter,
		status);
}
