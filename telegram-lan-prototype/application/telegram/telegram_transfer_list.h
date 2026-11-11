#pragma once
#include "telegram_original_controls.h"
#include "ui/widgets/scroll_area.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QHash>
#include <functional>
namespace TelegramLan {
class TransferListRow;
// LAN file state presenter using original lib_ui controls, not Telegram's
// Session-backed document-message/history class.
class TransferList final : public Ui::RpWidget {
public:
 explicit TransferList(QWidget *parent=nullptr);
 void upsert(QJsonObject snapshot);
 QWidget *rowWidget(const QString &id) const;
 std::function<void(QString action,QString id,QString path)> actionRequested;
private:
 void resizeEvent(QResizeEvent*) override;
 void relayout();
 Ui::ScrollArea *scroll_;
 Ui::RpWidget *content_;
 QHash<QString,TransferListRow*> rows_;
 QStringList order_;
};
QPointer<TransferList> ShowTransferBox(QWidget *parent,QJsonArray snapshots,
 std::function<void(QString action,QString id,QString path)> action);
}
