#include "new_group_box.h"

#include "avatar.h"
#include "theme.h"
#include "i18n.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

NewGroupBox::NewGroupBox(const QVector<Dialog> &peers, QWidget *parent)
	: OfficialBox(parent) {
	const auto &theme = Theme::current();
	setBoxTitle(i18n::tr("new_group"));
	setBoxWidth(theme.boxWideWidth + 16);

	name_ = new QLineEdit(body());
	name_->setPlaceholderText(i18n::tr("group_name"));
	name_->setFont(theme.fontText);
	name_->setFixedHeight(36);
	name_->setStyleSheet(
		QStringLiteral(
			"QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 8px; padding: 0 10px; }")
			.arg(theme.filterInputBg.name(), theme.windowFg.name(), theme.inputBorderFg.name()));
	bodyLayout()->addWidget(name_);

	auto *hint = new QLabel(i18n::tr("add_members"), body());
	hint->setFont(theme.fontStatus);
	hint->setStyleSheet(QStringLiteral("color: %1;").arg(theme.windowSubTextFg.name()));
	bodyLayout()->addWidget(hint);

	list_ = new QListWidget(body());
	list_->setSelectionMode(QAbstractItemView::NoSelection);
	list_->setMinimumHeight(220);
	list_->setStyleSheet(
		QStringLiteral("QListWidget { border: none; background: %1; color: %2; }")
			.arg(theme.boxBg.name(), theme.windowFg.name()));
	for (const auto &peer : peers) {
		if (peer.isGroup) {
			continue;
		}
		auto *item = new QListWidgetItem(peer.name, list_);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Unchecked);
		item->setData(Qt::UserRole, peer.id);
		item->setIcon(QIcon(makeAvatar(peer.id, peer.name, 36)));
		item->setSizeHint(QSize(0, 48));
	}
	bodyLayout()->addWidget(list_, 1);

	auto *create = addConfirmButton(i18n::tr("create"));
	addCancelButton(i18n::tr("cancel"));
	create->setDefault(true);
	create->setEnabled(false);
	connect(name_, &QLineEdit::textChanged, this, [create](const QString &text) {
		create->setEnabled(!text.trimmed().isEmpty());
	});
}

QString NewGroupBox::groupName() const { return name_->text().trimmed(); }

QStringList NewGroupBox::memberIds() const {
	QStringList ids;
	for (int i = 0; i < list_->count(); ++i) {
		auto *item = list_->item(i);
		if (item->checkState() == Qt::Checked) {
			ids.push_back(item->data(Qt::UserRole).toString());
		}
	}
	return ids;
}
