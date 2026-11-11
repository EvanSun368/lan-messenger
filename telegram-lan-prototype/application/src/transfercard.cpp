#include "transfercard.h"
#include <QtWidgets>
TransferCard::TransferCard(QWidget *parent):QWidget(parent) {
 setObjectName("transferCard");setMinimumWidth(220);setMaximumWidth(420);
 auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(5);
 m_name=new QLabel;m_name->setObjectName("transferName");m_name->setTextFormat(Qt::PlainText);m_name->setWordWrap(true);layout->addWidget(m_name);
 m_bytes=new QLabel;m_bytes->setObjectName("transferBytes");layout->addWidget(m_bytes);
 m_progress=new QProgressBar;m_progress->setObjectName("transferProgress");m_progress->setRange(0,1000);m_progress->setTextVisible(false);m_progress->setFixedHeight(5);layout->addWidget(m_progress);
 m_status=new QLabel;m_status->setObjectName("transferStatus");m_status->setTextFormat(Qt::PlainText);m_status->setWordWrap(true);layout->addWidget(m_status);
 auto *buttons=new QHBoxLayout;buttons->setContentsMargins(0,0,0,0);
 auto add=[&](const QString &name,const QString &text){auto *b=new QPushButton(text);b->setObjectName(name);buttons->addWidget(b);return b;};
 m_accept=add("transferAccept","接收");m_reject=add("transferReject","拒绝");m_pause=add("transferPause","暂停");m_retry=add("transferRetry","重试");m_open=add("transferOpen","打开文件");buttons->addStretch();layout->addLayout(buttons);
 connect(m_accept,&QPushButton::clicked,this,[this]{emit acceptRequested(m_snapshot["id"].toString());});
 connect(m_reject,&QPushButton::clicked,this,[this]{emit rejectRequested(m_snapshot["id"].toString());});
 connect(m_pause,&QPushButton::clicked,this,[this]{emit pauseRequested(m_snapshot["id"].toString());});
 connect(m_retry,&QPushButton::clicked,this,[this]{emit retryRequested(m_snapshot["id"].toString());});
 connect(m_open,&QPushButton::clicked,this,[this]{
  const QFileInfo file(m_snapshot["path"].toString());
  if(m_snapshot["direction"]=="in" && m_snapshot["state"]=="completed" && file.isFile() && !file.isSymLink()
    && file.size()==qint64(m_snapshot["total"].toDouble()))emit openRequested(m_snapshot["id"].toString(),file.absoluteFilePath());
  else {m_open->setEnabled(false);m_status->setText("本地文件已丢失或变更");}
 });
 setSnapshot({});
}
QString TransferCard::statusText(const QJsonObject &snapshot) {
 QString text=snapshot["status"].toString();
 if(snapshot["direction"]=="out") {
  const auto state=snapshot["state"].toString();
  if(state=="remote_completed")text="对方已接收并校验";
  else if(state=="remote_rejected")text="对方已拒绝";
  else if(state=="remote_failed")text="对方接收失败";
  else if(state=="remote_paused")text="对方已暂停";
  else if(state=="sent")text="已发送，最终结果未确认";
 }
 if(snapshot["remote_group_route"]=="unconfirmed")
  text+=" · 对方未确认群模式，旧版可能显示在单聊";
 if(!snapshot["persistence_error"].toString().isEmpty())text+=" · 状态未保存";
 return text;
}
void TransferCard::setSnapshot(QJsonObject snapshot) {
 m_snapshot=snapshot;setProperty("transferState",snapshot["state"].toString());setProperty("transferId",snapshot["id"].toString());
 const auto state=snapshot["state"].toString();const bool incoming=snapshot["direction"]=="in";
 const qint64 total=qMax<qint64>(0,qint64(snapshot["total"].toDouble()));
 const qint64 done=qBound<qint64>(0,qint64(snapshot["done"].toDouble()),total);
 m_name->setText("文件 · "+snapshot["name"].toString());
 m_bytes->setText(QString("%1 / %2 字节").arg(done).arg(total));
 m_progress->setValue(total?int(double(done)*1000/double(total)):(state=="completed"?1000:0));
 m_status->setText(statusText(snapshot));
 m_accept->setVisible(incoming && state=="waiting");m_reject->setVisible(incoming && state=="waiting");
 m_pause->setVisible(state=="running" || state=="verifying");
 // Sender-side pause does not control receiver restart; avoid a misleading retry action.
 const bool remoteRetry=!incoming && (state=="remote_failed" || state=="remote_paused");
 m_retry->setVisible(((state=="paused" || state=="failed") && (incoming || state=="failed")) || remoteRetry);
 m_retry->setText(remoteRetry?QStringLiteral("重新发送"):QStringLiteral("重试"));
 m_retry->setToolTip(remoteRetry?QStringLiteral("发送新的文件邀请，不会自动继续对方暂停的旧任务"):QString());
 m_retry->setEnabled(snapshot["can_retry"].toBool(!remoteRetry));
 m_open->setVisible(incoming && (state=="completed" || state=="missing"));
 const QFileInfo file(snapshot["path"].toString());
 const bool exists=incoming && state=="completed" && file.isFile() && !file.isSymLink() && file.size()==total;
 m_open->setEnabled(exists);
 if(incoming && state=="completed" && !exists)m_status->setText("本地文件已丢失或变更");
}
