#pragma once
#include "telegram_original_controls.h"
#include <QJsonArray>
#include <QPointer>
#include <functional>
namespace Ui {class InputField;class FlatLabel;class ScrollArea;class VerticalLayout;}
namespace TelegramLan {
struct SharingActions {
 std::function<void()> publish;
 std::function<void(QString)> copy,revoke;
 std::function<void(QString token,QString directory)> browse;
 std::function<void(QString token,QString directory)> inputsChanged;
 std::function<void(QString peer,QString token,QString directory)> browseFrozen;
 std::function<void(QString peer,QString token,QString relativeFile)> downloadFrozen;
};
class SharingPane final:public Ui::RpWidget {
public:
 SharingPane(QWidget *parent,SharingActions actions);
 void setRoots(QJsonArray roots);
 void setLoading(QString peer,QString token,QString directory);
 bool setRemote(QString peer,QString token,QString directory,QJsonArray entries,QString caption);
 QString tokenText()const;
 QString directoryText()const;
 void setInputs(QString token,QString directory);
protected:
 int resizeGetHeight(int width) override;
private:
 Ui::VerticalLayout *layout_;
 SharingActions actions_;
 Ui::InputField *token_,*directory_;
 Ui::FlatLabel *location_;
 Ui::ScrollArea *rootsScroll_,*remoteScroll_;
 QString peer_,requestToken_,requestDirectory_;
};
QPointer<SharingPane> ShowSharingBox(QWidget *parent,QJsonArray roots,SharingActions actions);
}
