#pragma once

#include "models.h"
#include "official_box.h"

class QLineEdit;
class QListWidget;

class NewGroupBox : public OfficialBox {
	Q_OBJECT
public:
	explicit NewGroupBox(const QVector<Dialog> &peers, QWidget *parent = nullptr);

	QString groupName() const;
	QStringList memberIds() const;

private:
	QLineEdit *name_ = nullptr;
	QListWidget *list_ = nullptr;
};
