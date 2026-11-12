#include "check.h"
#include "history_text.h"
#include "theme.h"

#include <QColor>
#include <QFont>
#include <QString>
#include <QImage>
#include <QPainter>
#include <QPoint>
#include <QRect>

namespace {

HistoryText::Metrics lay(
		HistoryText &text,
		int width,
		bool expanded,
		int maxLines = 50) {
	const auto &theme = Theme::light();
	return text.layout(
		width,
		expanded,
		maxLines,
		theme.fontText,
		theme.fontMono,
		theme.historyTextInFg,
		theme.historyLinkFg,
		theme.historyCodeBg,
		false,
		{},
		QStringLiteral("m1"));
}

QString manyLines(int count, const QString &line) {
	QString out;
	out.reserve((line.size() + 1) * count);
	for (int i = 0; i < count; ++i) {
		if (i) {
			out.append(QChar('\n'));
		}
		out.append(line);
	}
	return out;
}

} // namespace

LAN_TEST(history_text_collapsed_stops_at_official_line_budget) {
	HistoryText text;
	const auto source = manyLines(400, QStringLiteral("The official show-more budget is fifty lines."));
	text.setSource(source);
	LAN_CHECK_EQ(text.source().size(), source.size());

	const auto metrics = lay(text, 280, false, 50);
	LAN_CHECK(metrics.clipped);
	LAN_CHECK(text.lineCount() <= 50);
	LAN_CHECK(metrics.lineCount <= 50);
	LAN_CHECK(metrics.height <= 50 * metrics.lineH + metrics.lineH);
}

LAN_TEST(history_text_layout_is_cached_for_the_same_width) {
	HistoryText text;
	text.setSource(manyLines(120, QStringLiteral("cache me")));
	const auto first = lay(text, 260, false, 50);
	const int lines = text.lineCount();
	const auto second = lay(text, 260, false, 50);
	LAN_CHECK_EQ(first.height, second.height);
	LAN_CHECK_EQ(lines, text.lineCount());
	LAN_CHECK(first.clipped);
}

LAN_TEST(history_text_expanded_keeps_full_text_and_lays_out_all_lines) {
	HistoryText text;
	const auto source = manyLines(80, QStringLiteral("line body"));
	text.setSource(source);
	const auto collapsed = lay(text, 240, false, 50);
	LAN_CHECK(collapsed.clipped);
	LAN_CHECK(text.lineCount() <= 50);

	const auto expanded = lay(text, 240, true, 50);
	LAN_CHECK(!expanded.clipped);
	LAN_CHECK(text.lineCount() > 50);
	LAN_CHECK(expanded.height > collapsed.height);
	LAN_CHECK_EQ(text.source().size(), source.size());
}

LAN_TEST(history_text_markdown_entities_are_parsed_once_and_kept) {
	HistoryText text;
	const QString source = QStringLiteral("**bold** and https://example.com/docs\n")
		+ manyLines(80, QStringLiteral("more text"));
	text.setSource(source);
	lay(text, 300, false, 50);
	LAN_CHECK(!text.parsed().entities.isEmpty());
	const auto *first = &text.parsed();
	lay(text, 300, false, 50);
	LAN_CHECK(&text.parsed() == first);
}

LAN_TEST(history_text_collapsed_does_not_itemize_the_full_source) {
	HistoryText text;
	const auto source = manyLines(8000, QStringLiteral("word word word word word"));
	text.setSource(source);
	LAN_CHECK(source.size() > 100000);
	const auto metrics = lay(text, 280, false, 50);
	LAN_CHECK(metrics.clipped);
	LAN_CHECK(text.lineCount() <= 50);
	LAN_CHECK(text.laidOutTextLength() < source.size());
	LAN_CHECK(text.laidOutTextLength() < 20000);
}

LAN_TEST(history_text_layout_is_not_rebuilt_on_draw) {
	HistoryText text;
	text.setSource(manyLines(400, QStringLiteral("stable cache")));
	lay(text, 260, false, 50);
	const int generation = text.layoutGeneration();
	const int lines = text.lineCount();
	const int laid = text.laidOutTextLength();

	QImage image(300, 80, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::white);
	QPainter painter(&image);
	text.draw(&painter, QRect(0, 0, 260, 2000), QColor(Qt::black), QRect(0, 0, 260, 80));
	painter.end();

	LAN_CHECK_EQ(text.layoutGeneration(), generation);
	LAN_CHECK_EQ(text.lineCount(), lines);
	LAN_CHECK_EQ(text.laidOutTextLength(), laid);
	lay(text, 260, false, 50);
	LAN_CHECK_EQ(text.layoutGeneration(), generation);
}

LAN_TEST(history_text_draw_clip_builds_only_a_visible_line_window) {
	HistoryText text;
	text.setSource(manyLines(400, QStringLiteral("visible window line body")));
	const auto expanded = lay(text, 240, true, 50);
	LAN_CHECK(!expanded.clipped);
	LAN_CHECK(text.lineCount() > 200);
	LAN_CHECK(expanded.height > 200 * 8);

	QImage image(260, 48, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::white);
	QPainter painter(&image);
	const QRect full(0, 0, 240, expanded.height);
	text.draw(&painter, full, QColor(Qt::black), QRect(0, 0, 240, 48));
	painter.end();

	LAN_CHECK(text.paintLineWindow() > 0);
	LAN_CHECK(text.paintLineWindow() <= 8);
	LAN_CHECK(text.layoutGeneration() > 0);
}

LAN_TEST(history_text_hit_test_uses_only_the_line_under_the_cursor) {
	HistoryText text;
	const QString source = QStringLiteral("see https://example.com/docs\n")
		+ manyLines(300, QStringLiteral("padding line"));
	text.setSource(source);
	const auto metrics = lay(text, 280, true, 50);
	LAN_CHECK(text.lineCount() > 200);

	const auto spots = text.hotspots(QPoint(0, 0), QRect(0, 0, 280, metrics.lineH + 4));
	LAN_CHECK(!spots.isEmpty());
	LAN_CHECK(text.paintLineWindow() <= 4);

	const auto hit = text.hitTest(QPoint(0, 0), spots.first().rect.center());
	LAN_CHECK(!hit.url.isEmpty());
	LAN_CHECK(text.paintLineWindow() <= 4);
}
