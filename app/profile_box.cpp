#include "profile_box.h"

#include "avatar.h"
#include "i18n.h"
#include "theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>

static QLineEdit *makeLine(const QString &text, const QString &placeholder, QWidget *parent, bool enabled) {
	const auto &theme = Theme::current();
	auto *edit = new QLineEdit(text, parent);
	edit->setPlaceholderText(placeholder);
	edit->setFont(theme.fontText);
	edit->setFixedHeight(36);
	edit->setEnabled(enabled);
	edit->setStyleSheet(
		QStringLiteral(
			"QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 8px; padding: 0 10px; }")
			.arg(theme.filterInputBg.name(), theme.windowFg.name(), theme.inputBorderFg.name()));
	return edit;
}

ProfileBox::ProfileBox(
		const QString &id,
		const QString &firstName,
		const QString &lastName,
		const QString &bio,
		const QString &avatarPath,
		const QString &ip,
		bool editable,
		QWidget *parent)
	: OfficialBox(parent)
	, id_(id)
	, avatarPath_(avatarPath)
	, editable_(editable) {
	const auto &theme = Theme::current();
	setBoxTitle(editable_ ? i18n::tr("edit_profile") : (firstName.isEmpty() ? id : firstName));
	setBoxWidth(theme.boxWideWidth + 36);

	auto *row = new QHBoxLayout;
	avatar_ = new QLabel(body());
	avatar_->setFixedSize(72, 72);
	if (editable_) {
		avatar_->setCursor(Qt::PointingHandCursor);
	}
	row->addWidget(avatar_);
	if (editable_) {
		auto *change = new QPushButton(i18n::tr("set_photo"), body());
		change->setCursor(Qt::PointingHandCursor);
		change->setStyleSheet(
			QStringLiteral("QPushButton { color: %1; border: none; background: transparent; }")
				.arg(theme.windowActiveTextFg.name()));
		connect(change, &QPushButton::clicked, this, &ProfileBox::chooseAvatar);
		row->addWidget(change);
	}
	row->addStretch();
	bodyLayout()->addLayout(row);

	first_ = makeLine(firstName, i18n::tr("first_name"), body(), editable_);
	last_ = makeLine(lastName, i18n::tr("last_name"), body(), editable_);
	bodyLayout()->addWidget(first_);
	bodyLayout()->addWidget(last_);

	bio_ = new QPlainTextEdit(bio, body());
	bio_->setPlaceholderText(i18n::tr("bio"));
	bio_->setFont(theme.fontText);
	bio_->setFixedHeight(84);
	bio_->setEnabled(editable_);
	bio_->setStyleSheet(
		QStringLiteral(
			"QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 8px; padding: 6px 10px; }")
			.arg(theme.filterInputBg.name(), theme.windowFg.name(), theme.inputBorderFg.name()));
	bodyLayout()->addWidget(bio_);

	if (!ip.isEmpty()) {
		auto *ipLabel = new QLabel(i18n::tr("ip_label") + QStringLiteral(": ") + ip, body());
		ipLabel->setFont(theme.fontStatus);
		ipLabel->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
		ipLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		bodyLayout()->addWidget(ipLabel);
	}

	if (editable_) {
		auto *hint = new QLabel(i18n::tr("profile_hint"), body());
		hint->setWordWrap(true);
		hint->setFont(theme.fontStatus);
		hint->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
		bodyLayout()->addWidget(hint);
		auto *save = addConfirmButton(i18n::tr("save"));
		addCancelButton(i18n::tr("cancel"));
		save->setDefault(true);
		connect(first_, &QLineEdit::textChanged, this, [this, save](const QString &) {
			save->setEnabled(!displayName().isEmpty());
			refreshAvatar();
		});
		connect(last_, &QLineEdit::textChanged, this, [this, save](const QString &) {
			save->setEnabled(!displayName().isEmpty());
			refreshAvatar();
		});
	} else {
		addCloseButton(i18n::tr("ok"));
	}
	refreshAvatar();
}

void ProfileBox::chooseAvatar() {
	const auto path = QFileDialog::getOpenFileName(
		this,
		i18n::tr("choose_photo"),
		QString(),
		i18n::tr("images_filter"));
	if (path.isEmpty()) {
		return;
	}
	avatarPath_ = path;
	refreshAvatar();
}

void ProfileBox::refreshAvatar() {
	avatar_->setPixmap(makeAvatar(id_, displayName(), 72, false, avatarPath_));
}

QString ProfileBox::firstName() const { return first_->text().trimmed(); }
QString ProfileBox::lastName() const { return last_->text().trimmed(); }
QString ProfileBox::bio() const { return bio_->toPlainText().trimmed(); }
QString ProfileBox::displayName() const {
	const auto first = firstName();
	const auto last = lastName();
	if (first.isEmpty()) return last;
	if (last.isEmpty()) return first;
	return first + QChar(' ') + last;
}
QString ProfileBox::avatarPath() const { return avatarPath_; }
