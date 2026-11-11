#pragma once
// New LAN message host around ORIGINAL Ui::FlatLabel / Ui::Text::String.
// Upstream rendering sources are linked unchanged from GPL lib_ui.
#include <QWidget>
#include <QFont>
#include <QStringList>
#include <memory>
class QToolButton;
namespace Ui { class FlatLabel; }
namespace style { struct FlatLabel; }
class TelegramMessageText final : public QWidget {
 Q_OBJECT
public:
 TelegramMessageText(QString text,QFont font,QWidget *parent=nullptr);
 ~TelegramMessageText() override;
 QString toPlainText() const {return text_;}
 void applyFont(const QFont &font);
 void setTextWidthLimit(int width);
 void setCollapseLongMessages(bool enabled);
 void setExpanded(bool expanded);
 bool isExpanded() const {return expanded_;}
 bool isCollapsible() const {return collapseEnabled_ && fullHeight_>collapsedHeight_;}
 bool saveFullTextToFile(const QString &path) const;
 void copyAll() const;
 QList<Ui::FlatLabel*> sourceLabels() const {return labels_;}
 static QStringList splitForSourceEngine(const QString &text);
protected:
 void contextMenuEvent(QContextMenuEvent *event) override;
 void keyPressEvent(QKeyEvent *event) override;
private:
 void rebuild(const QFont &font);
 void applyCollapse();
 void saveAll();
 QWidget *viewport_=nullptr,*content_=nullptr;
 QToolButton *toggle_=nullptr;
 bool collapseEnabled_=false,expanded_=false;
 int fullHeight_=0,collapsedHeight_=0,contentWidth_=0;
 QString text_;
 QFont font_;
 int widthLimit_=320;
 std::unique_ptr<style::FlatLabel> style_;
 QList<Ui::FlatLabel*> labels_;
};
