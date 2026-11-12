#pragma once

#include "markdown.h"

#include <QColor>
#include <QFont>
#include <QRect>
#include <QSet>
#include <QString>
#include <QTextLayout>
#include <QVector>

class QPainter;

// Slim counterpart of official Ui::Text::String: parse once, break lines
// once for a width, then paint only the lines that intersect the clip
// (official draw's yFrom/yTo). Collapsed bubbles stop at the official
// 50-line budget and never itemize the rest of a huge source.
class HistoryText {
public:
	struct Metrics {
		int height = 0;
		int fullHeight = 0;
		bool clipped = false;
		int lineH = 0;
		int lineCount = 0;
	};

	struct Hotspot {
		QRect rect;
		QString url;
		QString spoilerKey;
	};

	void setSource(const QString &source);
	[[nodiscard]] const QString &source() const { return source_; }
	[[nodiscard]] const markdown::ParsedText &parsed() const { return parsed_; }

	Metrics layout(
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
		const QString &msgId);

	void draw(
		QPainter *painter,
		const QRect &rect,
		const QColor &color,
		const QRect &clip = QRect()) const;
	[[nodiscard]] QVector<Hotspot> hotspots(
		const QPoint &origin,
		const QRect &clip = QRect()) const;
	[[nodiscard]] Hotspot hitTest(const QPoint &origin, const QPoint &pos) const;
	[[nodiscard]] int lineCount() const;
	[[nodiscard]] int laidOutTextLength() const { return laidOutTextLength_; }
	[[nodiscard]] int paintLineWindow() const { return paintLineWindow_; }
	[[nodiscard]] int layoutGeneration() const { return generation_; }
	void invalidatePaint();

private:
	struct LineRec {
		int start = 0;
		int length = 0;
		int y = 0;
		int height = 0;
	};

	void rebuildLayout(
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
		const QString &msgId);
	void clearPaintWindow() const;
	void ensurePaintWindow(int fromLine, int tillLine) const;
	[[nodiscard]] int firstLineAt(int y) const;
	[[nodiscard]] int lastLineBefore(int y) const;

	QString source_;
	markdown::ParsedText parsed_;
	QString prepared_;
	QFont font_;
	QFont mono_;
	QVector<LineRec> lines_;
	Metrics metrics_;
	QColor color_;
	QColor linkColor_;
	QColor codeBg_;
	int width_ = 0;
	int maxLines_ = 0;
	int laidOutTextLength_ = 0;
	int generation_ = 0;
	bool expanded_ = false;
	bool dark_ = false;
	bool laidOut_ = false;
	QString msgId_;
	QSet<QString> revealed_;

	mutable QTextLayout paintLayout_;
	mutable int paintFrom_ = -1;
	mutable int paintTill_ = -1;
	mutable int paintLineWindow_ = 0;
};
