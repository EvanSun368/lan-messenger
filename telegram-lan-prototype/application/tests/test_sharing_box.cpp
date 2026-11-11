#include <QtTest>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_sharing_box.h"
#include "ui/widgets/fields/input_field.h"
#endif
class SharingBoxTest:public QObject {
 Q_OBJECT
private slots:
 void noImplicitPublishAndFrozenRemoteContext() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Original sharing box requires FULL");
#else
  ensureTelegramUiRuntime();QWidget parent;parent.resize(800,850);parent.show();
  int published=0;QString copied,revoked;QStringList browsed,downloaded,entered,draft;
  int browseCount=0,draftCount=0;
  TelegramLan::SharingActions actions;
  actions.publish=[&]{++published;};actions.copy=[&](QString t){copied=t;};actions.revoke=[&](QString t){revoked=t;};
  actions.browse=[&](QString t,QString d){++browseCount;browsed={t,d};};
  actions.inputsChanged=[&](QString t,QString d){++draftCount;draft={t,d};};
  actions.browseFrozen=[&](QString p,QString t,QString d){entered={p,t,d};};
  actions.downloadFrozen=[&](QString p,QString t,QString d){downloaded={p,t,d};};
  auto pane=TelegramLan::ShowSharingBox(&parent,QJsonArray{QJsonObject{{"path","/local/shared"},{"token","root-token"}}},actions);
  QVERIFY(pane);QTest::qWait(30);QCOMPARE(published,0);QVERIFY(downloaded.isEmpty());
  auto *publish=pane->findChild<QWidget*>("originalSharePublish");QVERIFY(dynamic_cast<Ui::LinkButton*>(publish));
  QTest::mouseClick(publish,Qt::LeftButton);QCOMPARE(published,1);
  auto *copy=pane->findChild<QWidget*>("originalShareCopy");QVERIFY(copy);QTest::mouseClick(copy,Qt::LeftButton);QCOMPARE(copied,QString("root-token"));
  auto *revoke=pane->findChild<QWidget*>("originalShareRevoke");QVERIFY(revoke);QTest::mouseClick(revoke,Qt::LeftButton);QCOMPARE(revoked,QString("root-token"));
  pane->setInputs("token-A","folder");
  QVERIFY(draftCount>0);QCOMPARE(draft,QStringList({"token-A","folder"}));
  QCOMPARE(browseCount,0);QVERIFY(downloaded.isEmpty());QVERIFY(entered.isEmpty());
  QTest::mouseClick(pane->findChild<QWidget*>("originalShareBrowse"),Qt::LeftButton);
  QCOMPARE(browsed,QStringList({"token-A","folder"}));
  pane->setLoading("peer-A","token-A","folder");
  QJsonArray entries{QJsonObject{{"name","document.txt"},{"directory",false}},QJsonObject{{"name","child"},{"directory",true}}};
  QVERIFY(!pane->setRemote("peer-B","token-A","folder",entries,"stale"));
  QVERIFY(!pane->findChild<QWidget*>("originalShareDownload"));
  QVERIFY(pane->setRemote("peer-A","token-A","folder",entries,"已读取"));
  const int requestsBeforeEdit=browseCount;
  pane->setInputs("edited-token","edited-folder");
  QCOMPARE(draft,QStringList({"edited-token","edited-folder"}));
  QCOMPARE(browseCount,requestsBeforeEdit);QVERIFY(downloaded.isEmpty());
  QTest::mouseClick(pane->findChild<QWidget*>("originalShareDownload"),Qt::LeftButton);
  QCOMPARE(downloaded,QStringList({"peer-A","token-A","folder/document.txt"}));
  QTest::mouseClick(pane->findChild<QWidget*>("originalShareEnter"),Qt::LeftButton);
  QCOMPARE(entered,QStringList({"peer-A","token-A","folder/child"}));
  parent.resize(400,850);QTest::qWait(50);
  for(const auto &name:{"originalSharePublish","originalShareBrowse","originalShareDownload","originalShareEnter"}) {
    auto *control=pane->findChild<QWidget*>(name);QVERIFY(control);QVERIFY(control->isVisible());
    QVERIFY(control->width()>0);QVERIFY(control->height()>0);
    QVERIFY(control->visibleRegion().contains(control->rect().center()));
    const auto point=control->mapTo(&parent,QPoint());
    QVERIFY(point.x()>=0);QVERIFY(point.x()+control->width()<=parent.width());
  }
  published=0;QTest::mouseClick(pane->findChild<QWidget*>("originalSharePublish"),Qt::LeftButton);QCOMPARE(published,1);
  downloaded.clear();QTest::mouseClick(pane->findChild<QWidget*>("originalShareDownload"),Qt::LeftButton);
  QCOMPARE(downloaded,QStringList({"peer-A","token-A","folder/document.txt"}));
  QVERIFY(parent.grab().save("/tmp/standalone-original-sharing-box-narrow.png"));
  pane->setLoading("peer-C","token-C","");
  QVERIFY(!pane->setRemote("peer-A","token-A","folder",entries,"late old response"));
  QVERIFY(!pane->findChild<QWidget*>("originalShareDownload"));
  parent.grab().save("/tmp/standalone-original-sharing-box.png");
  auto *close=parent.findChild<QWidget*>("originalShareClose");QVERIFY(close);QTest::mouseClick(close,Qt::LeftButton);QTRY_VERIFY(pane.isNull());
#endif
 }
};
QTEST_MAIN(SharingBoxTest)
#include "test_sharing_box.moc"
