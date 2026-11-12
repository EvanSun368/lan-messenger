#include "app_settings.h"

#include <QSettings>
#include <QString>

QString settingsPath(const std::filesystem::path &dir) {
	return QString::fromStdString((dir / "settings.ini").u8string());
}

static QString languageKey(Language language) {
	switch (language) {
	case Language::English:
		return QStringLiteral("en");
	case Language::ChineseSimplified:
		return QStringLiteral("zh-Hans");
	case Language::ChineseTraditional:
		return QStringLiteral("zh-Hant");
	case Language::System:
	default:
		return QStringLiteral("system");
	}
}

static Language languageFromKey(const QString &key) {
	if (key == QStringLiteral("en")) {
		return Language::English;
	}
	if (key == QStringLiteral("zh-Hans") || key == QStringLiteral("zh")
		|| key == QStringLiteral("zh-CN") || key == QStringLiteral("zh_CN")) {
		return Language::ChineseSimplified;
	}
	if (key == QStringLiteral("zh-Hant") || key == QStringLiteral("zh-TW")
		|| key == QStringLiteral("zh_TW")) {
		return Language::ChineseTraditional;
	}
	return Language::System;
}

AppSettings AppSettings::load(const std::filesystem::path &dir) {
	QSettings ini(settingsPath(dir), QSettings::IniFormat);
	AppSettings settings;
	settings.language = languageFromKey(ini.value(QStringLiteral("language"), QStringLiteral("system")).toString());
	settings.dark = ini.value(QStringLiteral("dark"), false).toBool();
	settings.encryptFiles = ini.value(QStringLiteral("encrypt_files"), false).toBool();
	settings.saveFolder = ini.value(QStringLiteral("save_folder")).toString();
	return settings;
}

void AppSettings::save(const std::filesystem::path &dir) const {
	QSettings ini(settingsPath(dir), QSettings::IniFormat);
	ini.setValue(QStringLiteral("language"), languageKey(language));
	ini.setValue(QStringLiteral("dark"), dark);
	ini.setValue(QStringLiteral("encrypt_files"), encryptFiles);
	ini.setValue(QStringLiteral("save_folder"), saveFolder);
	ini.sync();
}
