#include "markdown.h"

#include <QRegularExpression>
#include <algorithm>
#include <limits>

namespace markdown {
namespace {

QString separators(const char *extra) {
	static const QString quotes = QString::fromUtf8(
		"\xC2\xAB\xC2\xBB\xE2\x80\x9C\xE2\x80\x9D\xE2\x80\x98\xE2\x80\x99\xE2\x80\xA6");
	return QString::fromUtf8(" \x10\n\r\t.,:;<>|'\"[]{}!?%^()-+=")
		+ QChar(0xfdd0)
		+ QChar(0xfdd1)
		+ QChar(QChar::ParagraphSeparator)
		+ QChar(QChar::LineSeparator)
		+ quotes
		+ QString::fromUtf8(extra);
}

bool isNewline(QChar ch) {
	return ch == QChar('\n') || ch == QChar('\r')
		|| ch == QChar(QChar::LineSeparator)
		|| ch == QChar(QChar::ParagraphSeparator);
}

bool escapedAt(const QString &text, int pos) {
	int slashes = 0;
	for (int i = pos - 1; i >= 0 && text[i] == QChar('\\'); --i) {
		++slashes;
	}
	return (slashes % 2) == 1;
}

struct TagRule {
	QString tag;
	Type type = Type::Bold;
	QString goodBefore;
	QString badAfter;
	QString goodAfter; // empty = official "always ok" (strike)
	bool finishByNewline = false;
	bool preNewline = false;
};

const QVector<TagRule> &rules() {
	static const QVector<TagRule> cached = {
		{QStringLiteral("```"), Type::Pre, separators("*~/"), QStringLiteral("`"), separators("*~/"), false, true},
		{QStringLiteral("**"), Type::Bold, separators("`~/"), QStringLiteral("*"), separators("`~/"), false, false},
		{QStringLiteral("__"), Type::Italic, separators("`*~/"), QStringLiteral("_"), separators("`*~/"), false, false},
		{QStringLiteral("~~"), Type::Strike, separators("`*~/"), QStringLiteral("~"), QString(), false, false},
		{QStringLiteral("||"), Type::Spoiler, separators("|*~/"), QStringLiteral("|"), separators("|*~/"), false, false},
		{QStringLiteral("`"), Type::Code, separators("*~/"), QStringLiteral("`\n\r"), separators("*~/"), true, false},
	};
	return cached;
}

// Official Markdown*GoodBefore lists are ASCII separators. For a Chinese
// UI also treat Unicode punctuation / space as a delimiter so
// **粗体**、__斜体__ works the same as **bold**, __italic__.
bool isSeparatorChar(QChar ch, const QString &listed) {
	if (listed.contains(ch)) {
		return true;
	}
	switch (ch.category()) {
	case QChar::Punctuation_Connector:
	case QChar::Punctuation_Dash:
	case QChar::Punctuation_Open:
	case QChar::Punctuation_Close:
	case QChar::Punctuation_FinalQuote:
	case QChar::Punctuation_InitialQuote:
	case QChar::Punctuation_Other:
	case QChar::Separator_Space:
	case QChar::Separator_Line:
	case QChar::Separator_Paragraph:
		return true;
	default:
		return false;
	}
}

bool isGoodBefore(const QString &text, int pos, const QString &good) {
	return pos <= 0 || isSeparatorChar(text[pos - 1], good);
}

bool isBadAfter(const QString &text, int pos, int tagLen, const QString &bad) {
	const int after = pos + tagLen;
	return after < text.size() && !bad.isEmpty() && bad.contains(text[after]);
}

bool isBadBefore(const QString &text, int pos, const QString &bad) {
	return pos > 0 && !bad.isEmpty() && bad.contains(text[pos - 1]);
}

bool isGoodAfter(const QString &text, int pos, int tagLen, const QString &good) {
	const int after = pos + tagLen;
	// Official strike uses an empty goodAfter (always allowed).
	return after >= text.size() || good.isEmpty() || isSeparatorChar(text[after], good);
}

bool canOpen(const QString &text, int pos, const TagRule &rule) {
	if (escapedAt(text, pos)) {
		return false;
	}
	if (rule.preNewline && pos > 0 && !isNewline(text[pos - 1])) {
		return false;
	}
	return isGoodBefore(text, pos, rule.goodBefore)
		&& !isBadAfter(text, pos, rule.tag.size(), rule.badAfter);
}

bool canClose(const QString &text, int pos, const TagRule &rule) {
	if (escapedAt(text, pos)) {
		return false;
	}
	return !isBadBefore(text, pos, rule.badAfter)
		&& isGoodAfter(text, pos, rule.tag.size(), rule.goodAfter);
}

int indexOfTag(const QString &text, int from, const QString &tag) {
	int pos = from;
	while (pos + tag.size() <= text.size()) {
		if (text.mid(pos, tag.size()) == tag) {
			return pos;
		}
		++pos;
	}
	return -1;
}

struct Region {
	int open = 0;
	int close = 0;      // start of closer tag
	int bodyFrom = 0;
	int bodyTill = 0;
	int openTill = 0;   // end of opener (including language/newline for pre)
	int closeTill = 0;
	Type type = Type::Bold;
	QString data;
};

int findCloser(const QString &text, int from, const TagRule &rule) {
	const int tagLen = rule.tag.size();
	int pos = from;
	while (pos + tagLen <= text.size()) {
		if (rule.finishByNewline && isNewline(text[pos])) {
			return -1;
		}
		if (text.mid(pos, tagLen) == rule.tag && canClose(text, pos, rule)) {
			return pos;
		}
		++pos;
	}
	return -1;
}

bool adjustPreBody(const QString &text, Region *region) {
	int bodyFrom = region->open + 3;
	int bodyTill = region->close;
	const auto body = text.mid(bodyFrom, bodyTill - bodyFrom);
	const int nl = body.indexOf(QChar('\n'));
	if (nl > 0) {
		const auto lang = body.left(nl).trimmed();
		bool ok = !lang.isEmpty();
		for (const QChar ch : lang) {
			if (!ch.isLetterOrNumber() && ch != QChar('-') && ch != QChar('_')) {
				ok = false;
				break;
			}
		}
		if (ok) {
			region->data = lang;
			bodyFrom += nl + 1;
		}
	} else if (nl == 0) {
		++bodyFrom;
	}
	if (region->data.isEmpty() && bodyFrom < bodyTill && isNewline(text[bodyFrom])) {
		++bodyFrom;
	}
	const int closer = region->close;
	if (bodyTill > bodyFrom && isNewline(text[bodyTill - 1])) {
		--bodyTill;
	}
	if (bodyTill <= bodyFrom) {
		return false;
	}
	region->openTill = bodyFrom;
	region->bodyFrom = bodyFrom;
	region->bodyTill = bodyTill;
	// Drop a trailing newline in front of the closer, as official
	// getTextWithAppliedMarkdown does for a ``` that sits on its own line.
	region->close = bodyTill;
	region->closeTill = closer + 3;
	return true;
}

// Official InputField walks tags by earliest opener. Nested emphasis is
// allowed; inline code and pre consume their interior.
QVector<Region> collectRegions(const QString &text) {
	QVector<Region> regions;
	QVector<bool> consumed(text.size(), false);
	const auto free = [&](int from, int till) {
		for (int i = from; i < till && i < consumed.size(); ++i) {
			if (consumed[i]) {
				return false;
			}
		}
		return true;
	};
	const auto eat = [&](int from, int till) {
		for (int i = from; i < till && i < consumed.size(); ++i) {
			consumed[i] = true;
		}
	};

	int cursor = 0;
	while (cursor < text.size()) {
		int bestPos = std::numeric_limits<int>::max();
		const TagRule *best = nullptr;
		for (const auto &rule : rules()) {
			int pos = cursor;
			while (pos + rule.tag.size() <= text.size()) {
				pos = indexOfTag(text, pos, rule.tag);
				if (pos < 0) {
					break;
				}
				if (free(pos, pos + rule.tag.size()) && canOpen(text, pos, rule)) {
					if (pos < bestPos
						|| (pos == bestPos && best && rule.tag.size() > best->tag.size())) {
						bestPos = pos;
						best = &rule;
					}
					break;
				}
				++pos;
			}
		}
		if (!best) {
			break;
		}
		const int close = findCloser(text, bestPos + best->tag.size(), *best);
		if (close < 0 || !free(bestPos + best->tag.size(), close + best->tag.size())) {
			cursor = bestPos + 1;
			continue;
		}
		Region region;
		region.open = bestPos;
		region.close = close;
		region.openTill = bestPos + best->tag.size();
		region.closeTill = close + best->tag.size();
		region.bodyFrom = region.openTill;
		region.bodyTill = close;
		region.type = best->type;
		if (best->type == Type::Pre && !adjustPreBody(text, &region)) {
			cursor = bestPos + 1;
			continue;
		}
		if (region.bodyTill <= region.bodyFrom) {
			cursor = bestPos + 1;
			continue;
		}
		regions.push_back(region);
		if (best->type == Type::Code || best->type == Type::Pre) {
			eat(region.open, region.closeTill);
		} else {
			eat(region.open, region.openTill);
			eat(region.close, region.closeTill);
		}
		cursor = bestPos + best->tag.size();
	}
	return regions;
}

struct LinkSpan {
	int open = 0;
	int mid = 0;
	int close = 0;
	QString url;
};

QVector<LinkSpan> collectLinks(const QString &text, const QVector<bool> &frozen) {
	QVector<LinkSpan> out;
	for (int i = 0; i < text.size(); ++i) {
		if (frozen[i] || text[i] != QChar('[') || escapedAt(text, i)) {
			continue;
		}
		int mid = -1;
		for (int j = i + 1; j + 1 < text.size(); ++j) {
			if (frozen[j] || text[j] == QChar('\n')) {
				break;
			}
			if (text[j] == QChar(']') && text[j + 1] == QChar('(') && !escapedAt(text, j)) {
				mid = j;
				break;
			}
		}
		if (mid < 0 || mid == i + 1) {
			continue;
		}
		int close = -1;
		for (int j = mid + 2; j < text.size(); ++j) {
			if (frozen[j] || text[j] == QChar('\n') || text[j] == QChar(' ')) {
				break;
			}
			if (text[j] == QChar(')') && !escapedAt(text, j)) {
				close = j;
				break;
			}
		}
		if (close < 0) {
			continue;
		}
		const auto url = text.mid(mid + 2, close - (mid + 2)).trimmed();
		if (url.isEmpty() || (url.indexOf(QChar('.')) < 0 && url.indexOf(QChar(':')) < 0)) {
			continue;
		}
		out.push_back({i, mid, close, url});
		i = close;
	}
	return out;
}

void collectBareUrls(const QString &text, const QVector<bool> &frozen, QVector<Entity> *out) {
	static const QRegularExpression re(
		QStringLiteral(R"((https?://|www\.)[^\s<>\[\]()]+)"),
		QRegularExpression::CaseInsensitiveOption);
	auto it = re.globalMatch(text);
	while (it.hasNext()) {
		const auto m = it.next();
		const int start = int(m.capturedStart());
		int length = int(m.capturedLength());
		bool blocked = false;
		for (int i = start; i < start + length && i < frozen.size(); ++i) {
			if (frozen[i]) {
				blocked = true;
				break;
			}
		}
		if (blocked) {
			continue;
		}
		while (length > 0) {
			const QChar ch = text[start + length - 1];
			if (QStringLiteral(".,:;!?)]}>'\"").contains(ch)) {
				--length;
				continue;
			}
			break;
		}
		if (length < 4) {
			continue;
		}
		QString url = text.mid(start, length);
		if (url.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)) {
			url = QStringLiteral("https://") + url;
		}
		out->push_back({Type::Url, start, length, url});
	}
}

bool isEscape(const QString &text, int i) {
	return text[i] == QChar('\\') && i + 1 < text.size()
		&& QStringLiteral("*_~`|[]()\\").contains(text[i + 1]);
}

} // namespace

ParsedText parse(const QString &source) {
	ParsedText result;
	if (source.isEmpty()) {
		return result;
	}

	const auto regions = collectRegions(source);
	QVector<bool> drop(source.size(), false);
	QVector<bool> frozen(source.size(), false);
	for (const auto &region : regions) {
		for (int i = region.open; i < region.openTill && i < drop.size(); ++i) {
			drop[i] = true;
		}
		for (int i = region.close; i < region.closeTill && i < drop.size(); ++i) {
			drop[i] = true;
		}
		if (region.type == Type::Code || region.type == Type::Pre) {
			for (int i = region.bodyFrom; i < region.bodyTill && i < frozen.size(); ++i) {
				frozen[i] = true;
			}
		}
	}

	const auto links = collectLinks(source, [&] {
		QVector<bool> blocked = frozen;
		for (int i = 0; i < source.size(); ++i) {
			if (drop[i]) {
				blocked[i] = true;
			}
		}
		return blocked;
	}());
	for (const auto &link : links) {
		if (link.open >= 0 && link.open < drop.size()) {
			drop[link.open] = true;
		}
		if (link.mid >= 0 && link.mid + 1 < drop.size()) {
			drop[link.mid] = true;
			drop[link.mid + 1] = true;
		}
		for (int i = link.mid + 2; i <= link.close && i < drop.size(); ++i) {
			drop[i] = true;
		}
	}

	QString stripped;
	stripped.reserve(source.size());
	QVector<int> dest(source.size() + 1, 0);
	int di = 0;
	for (int i = 0; i < source.size(); ++i) {
		dest[i] = di;
		if (drop[i]) {
			continue;
		}
		if (!frozen[i] && isEscape(source, i)) {
			continue;
		}
		stripped.append(source[i]);
		++di;
	}
	dest[source.size()] = di;
	result.text = stripped;

	auto mapped = [&](int from, int till, Type type, const QString &data) {
		const int nFrom = dest[qBound(0, from, source.size())];
		const int nTill = dest[qBound(0, till, source.size())];
		if (nTill > nFrom) {
			result.entities.push_back({type, nFrom, nTill - nFrom, data});
		}
	};

	for (const auto &region : regions) {
		mapped(region.bodyFrom, region.bodyTill, region.type, region.data);
	}
	for (const auto &link : links) {
		mapped(link.open + 1, link.mid, Type::Link, link.url);
	}

	QVector<bool> frozenOut(result.text.size(), false);
	for (const auto &e : result.entities) {
		if (e.type == Type::Code || e.type == Type::Pre || e.type == Type::Link) {
			for (int i = e.offset; i < e.offset + e.length && i < frozenOut.size(); ++i) {
				frozenOut[i] = true;
			}
		}
	}
	collectBareUrls(result.text, frozenOut, &result.entities);

	std::sort(result.entities.begin(), result.entities.end(),
		[](const Entity &a, const Entity &b) {
			if (a.offset != b.offset) {
				return a.offset < b.offset;
			}
			return a.length > b.length;
		});
	return result;
}

} // namespace markdown
