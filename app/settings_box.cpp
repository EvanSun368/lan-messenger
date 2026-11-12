#include "settings_box.h"

#include "i18n.h"
#include "theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

SettingsBox::SettingsBox(const AppSettings &settings, QWidget *parent)
	: OfficialBox(parent) {
	const auto &theme = Theme::current();
	setBoxTitle(i18n::tr("settings"));
	setBoxWidth(theme.boxWideWidth + 40);

	auto *langLabel = new QLabel(i18n::tr("language"), body());
	langLabel->setFont(theme.fontStatus);
	langLabel->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
	bodyLayout()->addWidget(langLabel);
	language_ = new QComboBox(body());
	const Language options[] = {
		Language::System,
		Language::English,
		Language::ChineseSimplified,
		Language::ChineseTraditional,
	};
	int current = 0;
	for (int i = 0; i < 4; ++i) {
		language_->addItem(i18n::languageName(options[i]), int(options[i]));
		if (options[i] == settings.language) {
			current = i;
		}
	}
	language_->setCurrentIndex(current);
	bodyLayout()->addWidget(language_);

	encrypt_ = new QCheckBox(i18n::tr("encrypt_files"), body());
	encrypt_->setChecked(settings.encryptFiles);
	bodyLayout()->addWidget(encrypt_);
	auto *hint = new QLabel(i18n::tr("encrypt_files_about"), body());
	hint->setWordWrap(true);
	hint->setFont(theme.fontStatus);
	hint->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
	bodyLayout()->addWidget(hint);

	auto *saveLabel = new QLabel(i18n::tr("save_folder"), body());
	saveLabel->setFont(theme.fontStatus);
	saveLabel->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
	bodyLayout()->addWidget(saveLabel);
	auto *saveRow = new QHBoxLayout();
	saveFolder_ = new QLineEdit(body());
	saveFolder_->setText(settings.saveFolder);
	saveFolder_->setPlaceholderText(i18n::tr("downloads"));
	auto *browse = new QPushButton(i18n::tr("choose_save_location"), body());
	browse->setCursor(Qt::PointingHandCursor);
	browse->setStyleSheet(
		QStringLiteral("QPushButton { color: %1; border: none; background: transparent; }")
			.arg(theme.windowActiveTextFg.name()));
	connect(browse, &QPushButton::clicked, this, [this]() {
		const auto start = saveFolder_->text().isEmpty() ? QString() : saveFolder_->text();
		const auto path = QFileDialog::getExistingDirectory(this, i18n::tr("save_folder"), start);
		if (!path.isEmpty()) {
			saveFolder_->setText(path);
		}
	});
	saveRow->addWidget(saveFolder_, 1);
	saveRow->addWidget(browse);
	bodyLayout()->addLayout(saveRow);
	auto *saveHint = new QLabel(i18n::tr("save_folder_about"), body());
	saveHint->setWordWrap(true);
	saveHint->setFont(theme.fontStatus);
	saveHint->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
	bodyLayout()->addWidget(saveHint);

	auto *save = addConfirmButton(i18n::tr("save"));
	addCancelButton(i18n::tr("cancel"));
	save->setDefault(true);
}

AppSettings SettingsBox::settings() const {
	AppSettings out;
	out.language = Language(language_->currentData().toInt());
	out.encryptFiles = encrypt_->isChecked();
	out.saveFolder = saveFolder_ ? saveFolder_->text().trimmed() : QString();
	out.dark = Theme::current().dark;
	return out;
}
