#include "bubble.h"
#include "node.h"
#include <QApplication>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QMessageBox>
#include <QTimer>
#include <QTextDocument>
class MessageBubble : public QWidget {
 QString text; bool outgoing; QTextDocument doc;
public:
 MessageBubble(QString t,bool o):text(std::move(t)),outgoing(o){
  doc.setPlainText(text); doc.setDefaultFont(QFont("sans-serif",11));
  setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
 }
 void resizeEvent(QResizeEvent *e) override {
  QWidget::resizeEvent(e); doc.setTextWidth(qMax(40.0,width()*0.72-24));
  setFixedHeight(qMax(40,int(doc.size().height())+28));
 }
 void paintEvent(QPaintEvent*) override {
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
  int w=qMin(width()-16,int(doc.textWidth())+24);
  QRect r(outgoing?width()-w-8:8,4,w,height()-8);
  PaintTelegramBubble(p,r,outgoing?QColor("#d9ecff"):Qt::white,outgoing);
  p.translate(r.left()+12,r.top()+10);doc.drawContents(&p);
 }
};
int main(int argc,char **argv){
 QApplication app(argc,argv); app.setApplicationName("LanLink-Telegram-Spike");
 QString root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
 Node node("LAN user",root); if(!node.start())return 2;
 QWidget win;win.setWindowTitle("LanLink · Telegram source integration prototype");win.resize(760,620);
 QVBoxLayout layout(&win);
 QLabel note("Source integration prototype: Telegram bubble algorithm + LanLink LAN transport.\nNot the full Telegram UI. Unencrypted: trusted LAN only. Port: "+QString::number(node.port()));
 note.setWordWrap(true);layout.addWidget(&note);
 QLineEdit host;host.setPlaceholderText("Peer IP:port (e.g. 192.168.1.12:45872)");layout.addWidget(&host);
 QPushButton connect("Connect");layout.addWidget(&connect);
 QScrollArea scroll;scroll.setWidgetResizable(true);QWidget content;QVBoxLayout messages(&content);messages.setAlignment(Qt::AlignTop);scroll.setWidget(&content);layout.addWidget(&scroll,1);
 QLineEdit input;input.setPlaceholderText("Message");layout.addWidget(&input);QPushButton send("Send");layout.addWidget(&send);
 QString peer;
 QObject::connect(&connect,&QPushButton::clicked,[&]{auto i=host.text().lastIndexOf(':');if(i>0)node.addPeer(host.text().left(i),host.text().mid(i+1).toUShort());});
 auto append=[&](QString t,bool outgoing){messages.addWidget(new MessageBubble(t,outgoing));};
 QObject::connect(&node,&Node::event,[&](QString kind,QJsonObject d){
  if(kind=="peer"){auto p=node.peers();if(!p.isEmpty()){peer=p.first().toObject().value("id").toString();note.setText("Connected LAN peer. Telegram-derived bubble renderer. No Telegram service connection.");}}
  if(kind=="message")append(d.value("text").toString(),d.value("direction").toString()=="out");
  if(kind=="error")QMessageBox::warning(&win,"LAN error",d.value("message").toString());
 });
 QObject::connect(&send,&QPushButton::clicked,[&]{if(peer.isEmpty()){QMessageBox::information(&win,"Connect first","Connect a LAN peer before sending.");return;}if(!node.sendMessage(peer,input.text()).isEmpty())input.clear();});
 win.show();
 if(app.arguments().contains("--screenshot")){append("Telegram source is actually used to paint these bubbles.",false);append("LAN transport remains local. This is the first integration slice, not a completed migration.",true);QTimer::singleShot(500,[&]{win.grab().save("prototype.png");app.quit();});}
 int result=app.exec();node.stop();return result;
}
