#pragma once

#include "official_box.h"
#include <QString>

class QLineEdit;
class QLabel;
class QPlainTextEdit;

class ProfileBox : public OfficialBox {
	Q_OBJECT
public:
	ProfileBox(
		const QString &id,
		const QString &firstName,
		const QString &lastName,
		const QString &bio,
		const QString &avatarPath,
		const QString &ip,
		bool editable,
		QWidget *parent = nullptr);

	QString firstName() const;
	QString lastName() const;
	QString bio() const;
	QString displayName() const;
	QString avatarPath() const;

private:
	void chooseAvatar();
	void refreshAvatar();

	QString id_;
	QString avatarPath_;
	bool editable_ = true;
	QLineEdit *first_ = nullptr;
	QLineEdit *last_ = nullptr;
	QPlainTextEdit *bio_ = nullptr;
	QLabel *avatar_ = nullptr;
};
