#pragma once
// Ordinary-row fork of Telegram dialogs_inner_widget.cpp. See
// DIALOGS_INNER_SOURCE_SCOPE.md for retained branches and replaced data boundary.
#include "telegram_original_controls.h"
#include "telegram_row_painter.h"
#include "ui/widgets/scroll_area.h"
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <functional>
#include <optional>

namespace TelegramLan {
class DialogsInner;
class DialogsView final : public Ui::ScrollArea {
public:
 using Present = std::function<std::optional<DialogPresentation>(const QModelIndex&)>;
 DialogsView(QWidget *parent,QAbstractItemModel *model,QItemSelectionModel *selection,Present present);
 ~DialogsView();
 void refresh(); // Also call after host filter changes that do not emit dataChanged.
 QRect rowRect(const QModelIndex &index) const; // viewport coordinates
 QModelIndex current() const;
 QWidget *innerWidget() const;
 std::function<void(QModelIndex)> activated;
private:
 DialogsInner *inner_;
};
}
