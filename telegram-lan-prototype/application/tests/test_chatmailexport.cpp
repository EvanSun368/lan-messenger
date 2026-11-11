#include "chatmailexport.h"
#include <QJsonObject>
#include <QtTest>
using namespace ChatMailExport;
class ChatMailExportTest:public QObject{
 Q_OBJECT
 QJsonObject message(QString id,QString peer="group:team"){return {{"id",id},{"peer_id",peer},{"sender_id","alice"},{"sender_name","小王"},{"timestamp",1762855200.125},{"direction","in"},{"text","正文 "+id}};}
private slots:
 void maximumChatTextFitsWithExportHeaders(){auto row=message("long");row["text"]=QString("x").repeated(128*1024);const auto r=build("group:team",{row},{"long"});QVERIFY(r.ok);QVERIFY(r.body.contains(row["text"].toString()));}

 void selectedOnlyAndStableOrder(){QJsonArray rows{message("a"),message("b"),message("c")};auto r=build("group:team",rows,{"c","a"},"研发群");QVERIFY(r.ok);QCOMPARE(r.selectedCount,2);QVERIFY(r.body.contains("小王 [alice]"));QVERIFY(r.body.contains(".125Z"));QVERIFY(r.body.indexOf("正文 a")<r.body.indexOf("正文 c"));QVERIFY(!r.body.contains("正文 b"));QCOMPARE(r.subject,QString("聊天记录：研发群"));}
 void failClosed(){auto row=message("a");QVERIFY(!build("group:team",{row},{}).ok);QVERIFY(!build("group:team",{row},{"a","a"}).ok);QVERIFY(!build("group:team",{row},{"missing"}).ok);QVERIFY(!build("group:other",{row},{"a"}).ok);QVERIFY(!build("group:team",{row,row},{"a"}).ok);row["conversation_id"]="group:other";QVERIFY(!build("group:team",{row},{"a"}).ok);row.remove("conversation_id");row.remove("peer_id");QVERIFY(!build("group:team",{row},{"a"}).ok);}
 void binaryOnlyExplainedAndPathsNeverCopied(){auto image=message("i");image["image_path"]="/private/secret/image.png";auto file=message("file:f");file["transfer_id"]="f";file["path"]="/private/secret/file.bin";auto r=build("group:team",{image,file},{"i","file:f"});QVERIFY(r.ok);QVERIFY(r.body.contains("未包含图片文件"));QVERIFY(r.body.contains("未附加文件"));QVERIFY(!r.body.contains("/private/"));}
 void limitsAndNoPartialDraft(){auto row=message("a");row["text"]=QString("中").repeated(100);auto r=build("group:team",{row},{"a"},"",100);QVERIFY(!r.ok);QVERIFY(r.body.isEmpty());QCOMPARE(r.selectedCount,0);QVERIFY(!build("group:team",{row},{"a"},"title\r\nBcc:evil").ok);row["sender_name"]="fake\nheader";QVERIFY(!build("group:team",{row},{"a"}).ok);}
 void missingTimeAndOutgoingSenderAreExplicit(){auto row=message("a");row.remove("timestamp");row.remove("sender_id");row.remove("sender_name");row["direction"]="out";auto r=build("group:team",{row},{"a"});QVERIFY(r.ok);QVERIFY(r.body.contains("时间未知"));QVERIFY(r.body.contains("记录未提供发送者 ID"));}
};
QTEST_MAIN(ChatMailExportTest)
#include "test_chatmailexport.moc"
