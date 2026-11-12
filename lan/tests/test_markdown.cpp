#include "check.h"
#include "markdown.h"

#include <string>

namespace {

std::string s(const QString &text) {
	return text.toStdString();
}

const markdown::Entity *findType(const markdown::ParsedText &parsed, markdown::Type type) {
	for (const auto &e : parsed.entities) {
		if (e.type == type) {
			return &e;
		}
	}
	return nullptr;
}

} // namespace

LAN_TEST(markdown_bold_italic_strike_and_code) {
	const auto parsed = markdown::parse(QStringLiteral("**bold** and __italic__ and ~~strike~~ and `code`"));
	LAN_CHECK_EQ(s(parsed.text), std::string("bold and italic and strike and code"));
	const auto *bold = findType(parsed, markdown::Type::Bold);
	const auto *italic = findType(parsed, markdown::Type::Italic);
	const auto *strike = findType(parsed, markdown::Type::Strike);
	const auto *code = findType(parsed, markdown::Type::Code);
	LAN_REQUIRE(bold && italic && strike && code);
	LAN_CHECK_EQ(s(parsed.text.mid(bold->offset, bold->length)), std::string("bold"));
	LAN_CHECK_EQ(s(parsed.text.mid(italic->offset, italic->length)), std::string("italic"));
	LAN_CHECK_EQ(s(parsed.text.mid(strike->offset, strike->length)), std::string("strike"));
	LAN_CHECK_EQ(s(parsed.text.mid(code->offset, code->length)), std::string("code"));
}

LAN_TEST(markdown_pre_block_and_language) {
	const auto parsed = markdown::parse(QStringLiteral("```cpp\nint x = 1;\n```"));
	LAN_CHECK_EQ(s(parsed.text), std::string("int x = 1;"));
	const auto *pre = findType(parsed, markdown::Type::Pre);
	LAN_REQUIRE(pre);
	LAN_CHECK_EQ(s(pre->data), std::string("cpp"));
	LAN_CHECK_EQ(s(parsed.text.mid(pre->offset, pre->length)), std::string("int x = 1;"));
}

LAN_TEST(markdown_link_and_spoiler) {
	const auto parsed = markdown::parse(QStringLiteral("see [docs](https://example.com) and ||secret||"));
	LAN_CHECK_EQ(s(parsed.text), std::string("see docs and secret"));
	const auto *link = findType(parsed, markdown::Type::Link);
	const auto *spoiler = findType(parsed, markdown::Type::Spoiler);
	LAN_REQUIRE(link && spoiler);
	LAN_CHECK_EQ(s(link->data), std::string("https://example.com"));
	LAN_CHECK_EQ(s(parsed.text.mid(link->offset, link->length)), std::string("docs"));
	LAN_CHECK_EQ(s(parsed.text.mid(spoiler->offset, spoiler->length)), std::string("secret"));
}

LAN_TEST(markdown_nested_bold_italic) {
	const auto parsed = markdown::parse(QStringLiteral("**bold __italic__ still**"));
	LAN_CHECK_EQ(s(parsed.text), std::string("bold italic still"));
	const auto *bold = findType(parsed, markdown::Type::Bold);
	const auto *italic = findType(parsed, markdown::Type::Italic);
	LAN_REQUIRE(bold && italic);
	LAN_CHECK_EQ(s(parsed.text.mid(bold->offset, bold->length)), std::string("bold italic still"));
	LAN_CHECK_EQ(s(parsed.text.mid(italic->offset, italic->length)), std::string("italic"));
}

LAN_TEST(markdown_code_does_not_parse_inner_markers) {
	const auto parsed = markdown::parse(QStringLiteral("`**not bold**`"));
	LAN_CHECK_EQ(s(parsed.text), std::string("**not bold**"));
	LAN_CHECK(findType(parsed, markdown::Type::Bold) == nullptr);
	const auto *code = findType(parsed, markdown::Type::Code);
	LAN_REQUIRE(code);
	LAN_CHECK_EQ(s(parsed.text.mid(code->offset, code->length)), std::string("**not bold**"));
}

LAN_TEST(markdown_unmatched_markers_stay) {
	const auto parsed = markdown::parse(QStringLiteral("**not closed and `also` plain"));
	LAN_CHECK_EQ(s(parsed.text), std::string("**not closed and also plain"));
	LAN_CHECK(findType(parsed, markdown::Type::Bold) == nullptr);
	LAN_REQUIRE(findType(parsed, markdown::Type::Code));
}

LAN_TEST(markdown_escapes_and_bare_url) {
	const auto parsed = markdown::parse(QStringLiteral("\\*\\*plain\\*\\* and https://example.com/path."));
	LAN_CHECK_EQ(s(parsed.text), std::string("**plain** and https://example.com/path."));
	LAN_CHECK(findType(parsed, markdown::Type::Bold) == nullptr);
	const auto *url = findType(parsed, markdown::Type::Url);
	LAN_REQUIRE(url);
	LAN_CHECK_EQ(s(parsed.text.mid(url->offset, url->length)), std::string("https://example.com/path"));
	LAN_CHECK_EQ(s(url->data), std::string("https://example.com/path"));
}

LAN_TEST(markdown_inline_code_does_not_cross_newline) {
	const auto parsed = markdown::parse(QStringLiteral("`open\nstill open`"));
	// Official typed markdown finishes inline code at newline and leaves
	// the opener unmatched, so the closer ` is also left as a marker.
	LAN_CHECK(parsed.text.contains(QChar('`')));
	LAN_CHECK(findType(parsed, markdown::Type::Code) == nullptr);
}

LAN_TEST(markdown_cjk_punctuation_emphasis) {
	const auto parsed = markdown::parse(QString::fromUtf8("**粗体**、__斜体__、~~删除线~~ 和 `code`"));
	LAN_CHECK_EQ(s(parsed.text), std::string("粗体、斜体、删除线 和 code"));
	const auto *bold = findType(parsed, markdown::Type::Bold);
	const auto *italic = findType(parsed, markdown::Type::Italic);
	const auto *strike = findType(parsed, markdown::Type::Strike);
	const auto *code = findType(parsed, markdown::Type::Code);
	LAN_REQUIRE(bold && italic && strike && code);
	LAN_CHECK_EQ(s(parsed.text.mid(bold->offset, bold->length)), std::string("粗体"));
	LAN_CHECK_EQ(s(parsed.text.mid(italic->offset, italic->length)), std::string("斜体"));
	LAN_CHECK_EQ(s(parsed.text.mid(strike->offset, strike->length)), std::string("删除线"));
}

LAN_TEST(markdown_cjk_letters_do_not_open_emphasis) {
	const auto parsed = markdown::parse(QString::fromUtf8("这是**粗体**文字"));
	LAN_CHECK_EQ(s(parsed.text), std::string("这是**粗体**文字"));
	LAN_CHECK(findType(parsed, markdown::Type::Bold) == nullptr);
}
