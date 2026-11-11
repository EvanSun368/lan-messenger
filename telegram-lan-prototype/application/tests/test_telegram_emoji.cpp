#include <QtTest>
#include <QLineEdit>
#include <QListWidget>
#include <cstring>
#include "emoji_suggestions.h"
#include "emojipicker.h"
class EmojiSourceTest : public QObject {
 Q_OBJECT
private slots:
 void officialDictionary() {
  for(const auto &q: {QString("smile"),QString(":smile"),QString("heart"),QString("dog")}) {
   const auto results=Ui::Emoji::GetSuggestions({reinterpret_cast<const unsigned short*>(q.utf16()),std::size_t(q.size())});
   QVERIFY(!results.empty());
   for(const auto &r:results) QVERIFY(r.emoji().size()>0);
  }
 }
 void searchSelectAndRestore() {
  EmojiPicker picker;
  auto *search=picker.findChild<QLineEdit*>("emojiSearch");
  auto *list=picker.findChild<QListWidget*>("emojiList");
  QVERIFY(search); QVERIFY(list);
  const int categoryCount=list->count(); QVERIFY(categoryCount>0);
  search->setText("heart"); QVERIFY(list->count()>5);
  auto *item=list->item(0); QVERIFY(!item->data(Qt::UserRole).toString().isEmpty());
  QSignalSpy sent(&picker,&EmojiPicker::selected);
  QVERIFY(QMetaObject::invokeMethod(list,"itemClicked",Qt::DirectConnection,Q_ARG(QListWidgetItem*,item)));
  QCOMPARE(sent.count(),1);
  search->setText("zzzzzzzznoemoji"); QCOMPARE(list->count(),0);
  search->clear(); QCOMPARE(list->count(),categoryCount);
 }
};
QTEST_MAIN(EmojiSourceTest)
#include "test_telegram_emoji.moc"
