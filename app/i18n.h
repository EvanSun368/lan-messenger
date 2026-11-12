#pragma once

#include <QString>
#include <QObject>

enum class Language { System, English, ChineseSimplified, ChineseTraditional };

class LanguageNotifier : public QObject {
	Q_OBJECT
signals:
	void changed();
};

namespace i18n {

void initFromLocale();
void setLanguage(Language language);
Language language();
Language resolved();
bool isChinese();
QString tr(const char *key);
QString localize(const QString &keyOrValue);
QString languageName(Language language);
LanguageNotifier &notifier();

} // namespace i18n
