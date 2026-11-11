#include "telegram_message_text.h"
#include "telegram_original_controls.h"
#include "ui/ui_pch.h"
#include "ui/widgets/labels.h"
#include "styles/style_widgets.h"
#include "ui/widgets/popup_menu.h"
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontInfo>
#include <QFontMetrics>
#include <QMenu>
#include <QTextBoundaryFinder>
#include <QVBoxLayout>
#include <QToolButton>
#include <QFileDialog>
#include <QSaveFile>
#include <QMessageBox>

TelegramMessageText::TelegramMessageText(QString text,QFont font,QWidget *parent)
 :QWidget(parent),text_(std::move(text)),font_(font),style_(std::make_unique<style::FlatLabel>(st::defaultFlatLabel)) {
 setObjectName("messageText");setFocusPolicy(Qt::StrongFocus);
 // FlatLabel treats minWidth=0 as kQFixedMax (single-line natural sizing).
 // Explicit finite minimum is required for genuine text engine wrapping.
 style_->minWidth=1;style_->maxHeight=0;
 auto *box=new QVBoxLayout(this);box->setContentsMargins(0,0,0,0);box->setSpacing(0);
 viewport_=new QWidget(this);viewport_->setObjectName("messageTextViewport");
 content_=new QWidget(viewport_);content_->setObjectName("completeMessageText");
 auto *contents=new QVBoxLayout(content_);contents->setContentsMargins(0,0,0,0);contents->setSpacing(0);
 box->addWidget(viewport_);
 toggle_=new QToolButton(this);toggle_->setObjectName("expandMessageText");toggle_->setFixedHeight(28);
 toggle_->setToolButtonStyle(Qt::ToolButtonTextOnly);toggle_->setAutoRaise(true);
 box->addWidget(toggle_,0,Qt::AlignLeft);
 connect(toggle_,&QToolButton::clicked,this,[this]{setExpanded(!expanded_);});
 rebuild(font);
}
TelegramMessageText::~TelegramMessageText() {
 // FlatLabel holds a const style reference, which must outlive child deletion.
 qDeleteAll(labels_);labels_.clear();
}
QStringList TelegramMessageText::splitForSourceEngine(const QString &text) {
 QStringList result;
 // Upstream BlockParser::parse limits a single text to 0x8000 UTF-16 units.
 // Keep well below that ceiling, and never split a surrogate/combining sequence.
 for(qsizetype start=0;start<text.size();) {
  auto end=qMin(start+16000,text.size());
  if(end<text.size()) {
   QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme,text);
   boundary.setPosition(end);
   if(!boundary.isAtBoundary())end=boundary.toPreviousBoundary();
   // Maliciously unbounded combining sequence: safe codepoint boundary fallback.
   if(end<=start) {end=qMin(start+16000,text.size());if(text.at(end-1).isHighSurrogate())--end;}
  }
  result.push_back(text.mid(start,end-start));start=end;
 }
 if(result.isEmpty())result.push_back(QString());
 return result;
}
void TelegramMessageText::rebuild(const QFont &font) {
 qDeleteAll(labels_);labels_.clear();
 auto flags=style::FontFlags();
 if(font.bold())flags|=style::FontFlag::Bold;
 if(font.italic())flags|=style::FontFlag::Italic;
 style_->style.font=style::font(QFontInfo(font).pixelSize(),flags,font.family());
 int natural=0;for(const auto &line:text_.split('\n'))natural=qMax(natural,QFontMetrics(font).horizontalAdvance(line));
 const int width=qBound(qMin(60,widthLimit_),natural+4,widthLimit_);int height=0;
 for(const auto &part:splitForSourceEngine(text_)) {
  auto *label=new Ui::FlatLabel(content_,part,*style_);
  label->setObjectName("telegramSourceMessageLabel");
  label->setSelectable(true);label->setBreakEverywhere(true);
  label->setDoubleClickSelectsParagraph(true);
  // Plain source text intentionally does not activate links. Existing LAN
  // preview controls remain the only route to external navigation/confirmation.
  label->setClickHandlerFilter([](const auto&,Qt::MouseButton){return false;});
  label->setContextCopyText("复制选中片段");
  label->setContextMenuHook([this,label](Ui::FlatLabel::ContextMenuRequest request) {
   label->fillContextMenu(request);
   request.menu->addAction("复制整条消息",[this]{copyAll();});
   request.menu->addAction("保存完整文本…",[this]{saveAll();});
  });
  label->resizeToWidth(width);label->setFixedWidth(width);
  content_->layout()->addWidget(label);labels_.push_back(label);height+=label->height();
 }
 contentWidth_=width;fullHeight_=qMax(1,height);
 collapsedHeight_=12*std::max(style_->style.lineHeight,style_->style.font->height);
 content_->setFixedSize(width,fullHeight_);content_->layout()->activate();
 applyCollapse();
}
void TelegramMessageText::applyFont(const QFont &font){font_=font;rebuild(font_);}
void TelegramMessageText::setTextWidthLimit(int width) {
 width=qMax(1,width);if(width==widthLimit_)return;
 widthLimit_=width;rebuild(font_);
}
void TelegramMessageText::setCollapseLongMessages(bool enabled) {
 if(collapseEnabled_==enabled)return;collapseEnabled_=enabled;applyCollapse();
}
void TelegramMessageText::setExpanded(bool expanded) {
 if(expanded_==expanded)return;expanded_=expanded;applyCollapse();
}
void TelegramMessageText::applyCollapse() {
 const bool canCollapse=isCollapsible();
 const int visibleHeight=(canCollapse&&!expanded_)?collapsedHeight_:fullHeight_;
 viewport_->setFixedSize(contentWidth_,visibleHeight);
 toggle_->setVisible(canCollapse);toggle_->setText(expanded_?"收起":"展开全文");
 toggle_->setAccessibleName(expanded_?"收起长消息":"展开完整消息");
 setFixedSize(contentWidth_,visibleHeight+(canCollapse?toggle_->height():0));
 updateGeometry();
}
bool TelegramMessageText::saveFullTextToFile(const QString &path) const {
 QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
 const auto bytes=text_.toUtf8();return file.write(bytes)==bytes.size()&&file.commit();
}
void TelegramMessageText::saveAll() {
 const auto path=QFileDialog::getSaveFileName(this,"保存完整消息",QString(),"文本文件 (*.txt)");
 if(!path.isEmpty()&&!saveFullTextToFile(path))QMessageBox::warning(this,"保存失败","无法保存完整消息，请检查目录权限或剩余空间。");
}
void TelegramMessageText::copyAll() const {QApplication::clipboard()->setText(text_);}
void TelegramMessageText::contextMenuEvent(QContextMenuEvent *event) {
 auto *menu=telegramPopupMenu(this);
 QPointer<TelegramMessageText> owner(this);
 menu->addAction("复制整条消息",[owner]{if(owner)owner->copyAll();});
 menu->addAction("保存完整文本…",[owner]{if(owner)owner->saveAll();});
 menu->popup(event->globalPos());event->accept();
}

void TelegramMessageText::keyPressEvent(QKeyEvent *event) {
 if(event->matches(QKeySequence::Copy)){copyAll();event->accept();return;}
 QWidget::keyPressEvent(event);
}
