#include "members_box.h"

#include "avatar.h"
#include "theme.h"
#include "i18n.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSet>

MembersBox::MembersBox(
	const Dialog &group,
	const QString &selfId,
	const QVector<Dialog> &peers,
	QWidget *parent)
	: OfficialBox(parent) {
	const auto &theme = Theme::current();
	setBoxTitle(i18n::tr("members"));
	setBoxWidth(theme.boxWideWidth + 16);

	auto *hint = new QLabel(group.name, body());
	hint->setFont(theme.fontStatus);
	hint->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
	bodyLayout()->addWidget(hint);

	list_ = new QListWidget(body());
	list_->setSelectionMode(QAbstractItemView::NoSelection);
	list_->setMinimumHeight(260);
	list_->setStyleSheet(
		QStringLiteral("QListWidget { border: none; background: %1; color: %2; }")
			.arg(theme.boxBg.name(), theme.windowFg.name()));

	QSet<QString> current;
	for (const auto &id : group.memberIds) {
		current.insert(id);
	}
	QVector<Dialog> rows = peers;
	for (const auto &id : group.memberIds) {
		if (id == selfId) {
			continue;
		}
		bool found = false;
		for (const auto &peer : rows) {
			if (peer.id == id) {
				found = true;
				break;
			}
		}
		if (!found) {
			Dialog stub;
			stub.id = id;
			stub.name = id.left(8);
			rows.push_back(stub);
		}
	}
	for (const auto &peer : rows) {
		if (peer.isGroup || peer.id == selfId) {
			continue;
		}
		auto *item = new QListWidgetItem(peer.name, list_);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(current.contains(peer.id) ? Qt::Checked : Qt::Unchecked);
		item->setData(Qt::UserRole, peer.id);
		item->setIcon(QIcon(makeAvatar(peer.id, peer.name, 36)));
		item->setSizeHint(QSize(0, 48));
	}
	bodyLayout()->addWidget(list_, 1);

	auto *save = addConfirmButton(i18n::tr("save"));
	addCancelButton(i18n::tr("cancel"));
	save->setDefault(true);
}

QStringList MembersBox::memberIds() const {
	QStringList ids;
	for (int i = 0; i < list_->count(); ++i) {
		auto *item = list_->item(i);
		if (item->checkState() == Qt::Checked) {
			ids.push_back(item->data(Qt::UserRole).toString());
		}
	}
	return ids;
}
