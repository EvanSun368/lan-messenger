#pragma once

#include "app_settings.h"
#include "official_box.h"

class QComboBox;
class QCheckBox;
class QLineEdit;

class SettingsBox : public OfficialBox {
	Q_OBJECT
public:
	explicit SettingsBox(const AppSettings &settings, QWidget *parent = nullptr);
	AppSettings settings() const;

private:
	QComboBox *language_ = nullptr;
	QCheckBox *encrypt_ = nullptr;
	QLineEdit *saveFolder_ = nullptr;
};
