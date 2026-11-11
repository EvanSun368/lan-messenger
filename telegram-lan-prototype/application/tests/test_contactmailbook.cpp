#include "contactmailbook.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_contact_boxes.h"
#endif
class ContactMailBookTests:public QObject {
 Q_OBJECT
private slots:
 void validationAndIndependentMappings(){
  QTemporaryDir dir;ContactMailBook book(dir.filePath("private-emails.json"));QString error;
  QVERIFY(book.load(&error));
  const QString alice(32,'a'),bob(32,'b');
  QVERIFY(book.setEmail(alice,"alice@example.com",&error));
  QVERIFY(book.setEmail(bob,"bob@internal",&error));
  QCOMPARE(book.email(alice),QString("alice@example.com"));
  QVERIFY(!book.setEmail("group:"+alice,"other@example.com",&error));
  for(ushort c=0;c<32;++c)QVERIFY(!ContactMailBook::validEmail("alice"+QString(QChar(c))+"@example.com"));
  for(auto invalid:{"a@example.com,b@example.com","Alice <a@example.com>","no-address","a@b\r\nBcc:x@y"})
   QVERIFY(!ContactMailBook::validEmail(invalid));
  ContactMailBook restored(dir.filePath("private-emails.json"));QVERIFY(restored.load());
  QCOMPARE(restored.email(alice),book.email(alice));QCOMPARE(restored.email(bob),book.email(bob));
  QVERIFY(restored.setEmail(alice,{}));QVERIFY(restored.email(alice).isEmpty());
  ContactMailBook deleted(dir.filePath("private-emails.json"));QVERIFY(deleted.load());
  QVERIFY(deleted.email(alice).isEmpty());QCOMPARE(deleted.email(bob),QString("bob@internal"));
 }
 void failurePreservesLastKnownMapping(){
  QTemporaryDir dir;const auto path=dir.filePath("private-emails.json");ContactMailBook book(path);
  const QString id(32,'c');QVERIFY(book.setEmail(id,"known@example.com"));
  QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("{broken");file.close();
  QVERIFY(!book.load());QCOMPARE(book.email(id),QString("known@example.com"));
  QVERIFY(QFile::remove(path));QVERIFY(QDir().mkdir(path));
  QString error;QVERIFY(!book.setEmail(id,"new@example.com",&error));
  QCOMPARE(book.email(id),QString("known@example.com"));QVERIFY(!error.isEmpty());
 }
 void actualProfileEditingRemainsLocalAndKeepsFailedDraft(){
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Original profile editor requires FULL mode");
#else
  QWidget parent;parent.resize(800,700);parent.show();
  const QString peer(32,'d');int saves=0;bool success=false;QString value;
  QPointer<TelegramContactBoxHost> host=telegramShowContactProfile(&parent,peer,{{"nickname","Test"}},
   "old@example.com",[&](QString text){++saves;value=text;return success;});
  QCOMPARE(host->property("peerId").toString(),peer);
  auto input=dynamic_cast<Ui::InputField*>(host->findChild<QWidget*>("telegramContactLocalEmail"));
  auto save=host->findChild<QWidget*>("telegramContactLocalEmailSave");QVERIFY(input);QVERIFY(save);
  input->setText("invalid");QTest::mouseClick(save,Qt::LeftButton);QCOMPARE(saves,0);
  input->setText("new@example.com");QTest::mouseClick(save,Qt::LeftButton);QCOMPARE(saves,1);
  QVERIFY(host && host->isVisible());QCOMPARE(input->getLastText(),QString("new@example.com"));
  success=true;input->clear();QTest::mouseClick(save,Qt::LeftButton);
  QCOMPARE(saves,2);QVERIFY(value.isEmpty());QVERIFY(host && host->isVisible());
  host->closeBox();QTRY_VERIFY(host.isNull());
#endif
 }
};
QTEST_MAIN(ContactMailBookTests)
#include "test_contactmailbook.moc"
