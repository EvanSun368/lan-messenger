#pragma once
#include <QJsonObject>
#include <QString>
namespace DeliveryPresentation {
inline QString summary(const QJsonObject &s) {
 if(!s["known"].toBool())return QStringLiteral("送达状态未知");
 const int total=s["total"].toInt(),received=s["received"].toInt(),rejected=s["rejected"].toInt();
 if(total==1) {
  if(received==1)return QStringLiteral("对方已接收");
  if(rejected==1)return QStringLiteral("对方拒收 · 可重试");
  return QStringLiteral("等待发送／确认");
 }
 return QStringLiteral("已接收 %1/%2 台设备%3").arg(received).arg(total)
   .arg(rejected?QStringLiteral(" · %1 台拒收").arg(rejected):QString());
}
inline QString targetState(const QString &state) {
 if(state=="received")return QStringLiteral("对方已接收");
 if(state=="rejected")return QStringLiteral("对方拒收");
 if(state=="queued")return QStringLiteral("等待发送／确认");
 return QStringLiteral("未知");
}
}
