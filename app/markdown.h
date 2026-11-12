#pragma once

#include <QString>
#include <QVector>

// Official Telegram Desktop typed-markdown entities (InputField tags),
// plus [text](url) and bare URLs. The wire text stays the markdown source.
namespace markdown {

enum class Type {
	Bold,
	Italic,
	Strike,
	Code,
	Pre,
	Spoiler,
	Link,
	Url,
};

struct Entity {
	Type type = Type::Bold;
	int offset = 0;
	int length = 0;
	QString data; // link/url target, or pre language
};

struct ParsedText {
	QString text;
	QVector<Entity> entities;
};

ParsedText parse(const QString &source);

} // namespace markdown
