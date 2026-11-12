#pragma once

#include "i18n.h"

#include <filesystem>
#include <QString>

struct AppSettings {
	Language language = Language::System;
	bool dark = false;
	bool encryptFiles = false;
	QString saveFolder;

	static AppSettings load(const std::filesystem::path &dir);
	void save(const std::filesystem::path &dir) const;
};

QString settingsPath(const std::filesystem::path &dir);
