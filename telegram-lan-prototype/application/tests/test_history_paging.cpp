#include "node.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <limits>
class HistoryPagingTest:public QObject {
 Q_OBJECT
 static bool insert(const QString &root,int first,int count,const QString &peer="peer-a",double overrideTime=-1) {
  const auto connection=QUuid::createUuid().toString();bool ok=true;
  {
   auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(root+"/history.sqlite");ok=db.open()&&db.transaction();
   QSqlQuery q(db);q.prepare("INSERT INTO messages(id,peer,direction,text,time) VALUES(?,?,?,?,?)");
   for(int i=first;ok&&i<first+count;++i){q.bindValue(0,QString("id%1").arg(i,6,10,QChar('0')));q.bindValue(1,peer);q.bindValue(2,"in");q.bindValue(3,QString("正文😀 %1").arg(i));q.bindValue(4,overrideTime>=0?overrideTime:double(i/300));ok=q.exec();}
   if(ok)ok=db.commit();else db.rollback();db.close();
  }
  QSqlDatabase::removeDatabase(connection);return ok;
 }
private slots:
 void stableTiesAndConcurrentInsert() {
  QTemporaryDir dir;Node node("a",dir.path());QVERIFY(insert(dir.path(),0,1205));
  auto page=node.historyPage("peer-a",{},127);QVERIFY(page.error.isEmpty());QCOMPARE(page.records.size(),127);QVERIFY(page.hasMore);
  auto all=page.records;auto cursor=page.nextBefore;
  QVERIFY(insert(dir.path(),999999,1,"peer-a",99999)); // Newer insertion cannot shift an exclusive older cursor.
  int iterations=0;
  while(page.hasMore){page=node.historyPage("peer-a",cursor,127);QVERIFY2(page.error.isEmpty(),qPrintable(page.error));
   auto combined=page.records;for(auto r:all)combined.append(r);all=combined;cursor=page.nextBefore;QVERIFY(++iterations<20);
  }
  QCOMPARE(all.size(),1205);QSet<QString> ids;
  for(int i=0;i<all.size();++i){const auto id=all[i].toObject()["id"].toString();QCOMPARE(id,QString("id%1").arg(i,6,10,QChar('0')));ids.insert(id);}
  QCOMPARE(ids.size(),1205);QCOMPARE(node.historyPage("peer-a").records.last().toObject()["id"].toString(),QString("id999999"));
 }
 void cursorSurvivesRestartAndInvalidIsExplicit() {
  QTemporaryDir dir;HistoryCursor cursor;QJsonArray expected;
  {Node node("a",dir.path());QVERIFY(insert(dir.path(),0,405));cursor=node.historyPage("peer-a",{},100).nextBefore;expected=node.historyPage("peer-a",cursor,100).records;}
  Node reopened("a",dir.path());QCOMPARE(reopened.historyPage("peer-a",cursor,100).records,expected);
  auto invalid=cursor;invalid.id="' OR 1=1 --";auto result=reopened.historyPage("peer-a",invalid);QVERIFY(!result.error.isEmpty());QVERIFY(result.records.isEmpty());
  invalid=cursor;invalid.timestamp=std::numeric_limits<double>::quiet_NaN();QVERIFY(!reopened.historyPage("peer-a",invalid).error.isEmpty());
  invalid=cursor;invalid.conversation="peer-b";QVERIFY(!reopened.historyPage("peer-a",invalid).error.isEmpty());
  QCOMPARE(reopened.historyPage("peer-a",{},1000).records.size(),405);
 }
 void fileMappingAndOtherConversation() {
  QTemporaryDir dir;Node node("a",dir.path());QVERIFY(insert(dir.path(),0,20));QVERIFY(insert(dir.path(),50,5,"peer-b"));
  const auto name=QUuid::createUuid().toString();{
   auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(dir.path()+"/history.sqlite");QVERIFY(db.open());QSqlQuery q(db);
   QVERIFY(q.exec("INSERT INTO messages VALUES('file:abc','peer-a','out','file.txt',100)"));
   QVERIFY(q.exec("INSERT INTO message_transfers VALUES('file:abc','abc')"));db.close();
  }QSqlDatabase::removeDatabase(name);
  const auto page=node.historyPage("peer-a",{},1);QCOMPARE(page.records.first().toObject()["transfer_id"].toString(),QString("abc"));
  QVERIFY(node.historyPage("peer-a",page.nextBefore,100).error.isEmpty());
  QCOMPARE(node.historyPage("peer-b").records.size(),5);
 }
};
QTEST_GUILESS_MAIN(HistoryPagingTest)
#include "test_history_paging.moc"
