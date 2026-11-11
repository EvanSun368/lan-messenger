#include "mailsyncstate.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
class MailSyncStateTest:public QObject {
 Q_OBJECT
private slots:
 void baselineAndDeduplication(){
  MailSyncState state;auto first=state.apply("a","123",{3,1,3});QVERIFY(first.ok);QVERIFY(first.baseline);QVERIFY(!first.reset);QVERIFY(first.newUids.isEmpty());
  auto next=state.apply("a","123",{5,1,4,5});QVERIFY(next.ok);QCOMPARE(next.newUids,QList<quint64>({4,5}));
  QVERIFY(state.apply("a","123",{}).newUids.isEmpty());QVERIFY(state.apply("a","123",{4,5}).newUids.isEmpty());
  QCOMPARE(state.apply("a","123",{7}).newUids,QList<quint64>({7}));
 }
 void emptyBaselineRolloverAndIdentity(){
  MailSyncState state;QVERIFY(state.apply("a","1",{}).baseline);QCOMPARE(state.apply("a","1",{1}).newUids,QList<quint64>({1}));
  auto rolled=state.apply("a","2",{1,2});QVERIFY(rolled.baseline);QVERIFY(rolled.reset);QVERIFY(rolled.newUids.isEmpty());
  QCOMPARE(state.validity("a"),QString("2"));QVERIFY(state.apply("b","2",{5}).baseline);
  state.remove("a");QVERIFY(state.validity("a").isEmpty());QVERIFY(state.apply("a","2",{5}).baseline);
 }
 void invalidResultsDoNotAdvance(){
  MailSyncState state;QVERIFY(state.apply("a","1",{1}).ok);
  for(const auto &invalid:QStringList{"","0","-1","4294967296","abc","1 2"})QVERIFY(!state.apply("a",invalid,{10}).ok);
  QVERIFY(!state.apply("a","2",{0}).ok);QVERIFY(!state.apply("a","2",{4294967296ULL}).ok);QVERIFY(!state.apply("","1",{}).ok);
  QCOMPARE(state.validity("a"),QString("1"));QCOMPARE(state.apply("a","1",{2}).newUids,QList<quint64>({2}));
  QVERIFY(!state.apply("a","1",QList<quint64>(MailSyncState::MaxUids+1,2)).ok);
 }
 void roundtripAndMalformedFileAreAtomic(){
  QTemporaryDir dir;const auto path=dir.filePath("sync.json");MailSyncState state(path);QString error;QVERIFY(state.load(&error));
  QVERIFY(state.apply("a","4294967295",{4294967295ULL}).ok);QVERIFY(state.apply("empty","1",{}).ok);QVERIFY(state.save(&error));
  MailSyncState loaded(path);QVERIFY(loaded.load(&error));QVERIFY(loaded.apply("a","4294967295",{4294967295ULL}).newUids.isEmpty());
  QCOMPARE(loaded.apply("empty","1",{1}).newUids,QList<quint64>({1}));
  QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("{broken");file.close();QVERIFY(!loaded.load(&error));QVERIFY(!error.isEmpty());QCOMPARE(loaded.validity("a"),QString("4294967295"));
  MailSyncState failed(dir.filePath("missing/sync.json"));QVERIFY(!failed.save(&error));
 }
 void boundsAndCanonicalValidity(){
  MailSyncState state;QVERIFY(state.apply("a","0001",{}).ok);QVERIFY(!state.apply("a","1",{}).baseline);
  for(int i=1;i<MailSyncState::MaxAccounts;++i)QVERIFY(state.apply(QString::number(i),"1",{}).ok);
  QVERIFY(!state.apply("overflow","1",{}).ok);QVERIFY(state.apply("a","1",{1}).ok);
 }
};
QTEST_GUILESS_MAIN(MailSyncStateTest)
#include "test_mailsyncstate.moc"
