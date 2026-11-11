/* Telegram Desktop-derived ordinary history list fork.
 * Upstream c4bd064bcc84a359179e191fa6dfd216acda504f, GPL-3.0-or-later.
 * See LAN_HISTORY_LIST_SOURCE.md for exact source and exclusions. */
#pragma once
#include "ui/rp_widget.h"
#include <QPainter>
#include <QString>
#include <memory>
#include <vector>
#include <functional>
#include <QVBoxLayout>
namespace TelegramLan {
// Explicit LAN boundary, not a substitute Main::Session or HistoryItem.
class HistoryElement {
public:
 explicit HistoryElement(QString id): id_(std::move(id)) {}
 virtual ~HistoryElement()=default;
 const QString &id()const{return id_;}
 int y()const{return y_;} int height()const{return height_;}
 bool pendingResize()const{return dirty_;}
 void setY(int y){y_=y;} void invalidate(){dirty_=true;}
 int resizeGetHeight(int width){height_=std::max(1,measure(width));dirty_=false;return height_;}
 virtual void draw(QPainter &p,const QRect &localClip)=0;
 virtual void place(const QRect &) {}
protected:
 virtual int measure(int width)=0;
private:
 QString id_; int y_=0,height_=1;bool dirty_=true;
};
struct HistoryScrollAnchor {QString id;int offset=0;};
class LanHistoryList final:public Ui::RpWidget {
public:
 explicit LanHistoryList(QWidget *parent=nullptr):Ui::RpWidget(parent){}
 // Reject ambiguous identities without replacing the previous list.
 bool replace(std::vector<std::unique_ptr<HistoryElement>> items,bool resize=true);
 int layoutElements(int width,QPoint origin={}) {origin_=origin;return resizeGetHeight(width);}
 void setVisibleRange(int top,int bottom){visibleTop_=top;visibleBottom_=bottom;update();}
 HistoryScrollAnchor captureAnchor()const;
 int restoreAnchor(const HistoryScrollAnchor &anchor)const;
 int findItemIndexByY(int y)const;
 HistoryElement *strictFindItemByY(int y)const;
 int contentHeight()const{return itemsHeight_;}
 std::vector<QString> visibleIds(const QRect &clip)const;
 void relayout(int width){resizeToWidth(width);}
protected:
 int resizeGetHeight(int newWidth)override;
 void paintEvent(QPaintEvent *event)override;
private:
 int itemTop(const HistoryElement *view)const{return origin_.y()+view->y();}
 std::vector<std::unique_ptr<HistoryElement>> items_;
 QPoint origin_;
 int itemsWidth_=-1,itemsHeight_=0,visibleTop_=0,visibleBottom_=0;
};
// Transitional real app adapter. Original list geometry drives existing Qt rows;
// text selection, links and transfer buttons remain native child interactions.
class LanHistoryLayout final:public QVBoxLayout {
public:
 explicit LanHistoryLayout(LanHistoryList *parent):QVBoxLayout(parent),list_(parent){}
 void setGeometry(const QRect &rect)override;
 QSize sizeHint()const override;
 QSize minimumSize()const override;
 bool hasHeightForWidth()const override{return true;}
 int heightForWidth(int width)const override;
private:
 LanHistoryList *list_;
 bool layingOut_=false;
};
}
