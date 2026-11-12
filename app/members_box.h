#pragma once

#include "models.h"
#include "official_box.h"
#include <QStringList>

class QListWidget;

class MembersBox : public OfficialBox {
	Q_OBJECT
public:
	MembersBox(
		const Dialog &group,
		const QString &selfId,
		const QVector<Dialog> &peers,
		QWidget *parent = nullptr);

	QStringList memberIds() const;

private:
	QListWidget *list_ = nullptr;
};
