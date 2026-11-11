#include <QtTest>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_profile_box.h"
#endif
class ProfileBoxTests : public QObject {
 Q_OBJECT
private slots:
 void originalBoxLifecycleAndSave() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Original lib_ui modal host requires FULL mode");
#else
  QWidget parent;parent.resize(800,760);parent.show();
  int saves=0;bool valid=false;QJsonObject saved;
  QPointer<TelegramProfileOverlay> overlay=telegramShowProfileBox(&parent,{{"nickname","Original"}},
    [&](QJsonObject value){++saves;saved=value;return valid;});
  QVERIFY(overlay);
  QCOMPARE(telegramShowProfileBox(&parent,{},[](auto){return true;}),overlay.data());
  auto field=dynamic_cast<Ui::InputField*>(overlay->findChild<QWidget*>("telegramProfile_nickname"));
  QVERIFY(field);field->setText("Edited");
  auto save=overlay->findChild<QWidget*>("telegramProfileSave");QVERIFY(save);
  QTest::mouseClick(save,Qt::LeftButton);QCOMPARE(saves,1);QVERIFY(overlay && overlay->isVisible());
  valid=true;QTest::mouseClick(save,Qt::LeftButton);
  QTRY_VERIFY(overlay.isNull());QCOMPARE(saves,2);QCOMPARE(saved["nickname"].toString(),QString("Edited"));
  overlay=telegramShowProfileBox(&parent,{},[&](auto){++saves;return true;});
  parent.resize(720,680);QCOMPARE(overlay->size(),parent.size());
  auto cancel=overlay->findChild<QWidget*>("telegramProfileCancel");QVERIFY(cancel);
  QTest::mouseClick(cancel,Qt::LeftButton);QTRY_VERIFY(overlay.isNull());QCOMPARE(saves,2);
#endif
 }
 void narrowWindowButtonsRemainReachable() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Original lib_ui modal host requires FULL mode");
#else
  QWidget parent;parent.resize(800,700);parent.show();
  int saves=0;TelegramSettingsActions actions;
  actions.chooseFont=[]{};actions.setNotifications=[](bool){};
  actions.versionNotice="LanLink 开发预览。仅在可信局域网使用，未加密、无设备身份认证。";
  QPointer<TelegramProfileOverlay> overlay=telegramShowProfileBox(&parent,
    {{"nickname","演示用户"},{"organization","研发团队"}},[&](auto){++saves;return false;},std::move(actions));
  auto content=overlay->findChild<QWidget*>("telegramSettingsBox");QVERIFY(content);
  auto layer=content->parentWidget();QVERIFY(layer);
  const auto layerFits=[&]{
   return parent.rect().contains(QRect(layer->mapTo(&parent,QPoint()),layer->size()));
  };
  QTRY_VERIFY(layerFits());QCOMPARE(layer->width(),440);
  auto field=dynamic_cast<Ui::InputField*>(overlay->findChild<QWidget*>("telegramProfile_nickname"));
  QVERIFY(field);field->setText("缩放保留草稿");
  parent.resize(400,700);
  QTRY_COMPARE(overlay->size(),QSize(400,700));
  QTRY_COMPARE(layer->width(),368);
  QTRY_VERIFY(layerFits());
  QTRY_VERIFY(content->width()<=parent.width());
  auto save=overlay->findChild<QWidget*>("telegramProfileSave");
  auto cancel=overlay->findChild<QWidget*>("telegramProfileCancel");
  QVERIFY(save);QVERIFY(cancel);
  QTRY_VERIFY(save->isVisible() && cancel->isVisible());
  const auto inside=[&](QWidget *widget) {
   const QRect visible(widget->mapTo(&parent,QPoint()),widget->size());
   return parent.rect().contains(visible) && !widget->visibleRegion().isEmpty();
  };
  QVERIFY2(inside(save),"Save button must fit inside the 400px host");
  QVERIFY2(inside(cancel),"Cancel button must fit inside the 400px host");
  // Physical parent hit testing catches a button behind/clipped by the layer.
  const auto point=save->mapTo(&parent,save->rect().center());
  auto hit=parent.childAt(point);
  QVERIFY(hit && (hit==save || save->isAncestorOf(hit)));
  QTest::mouseClick(save,Qt::LeftButton);QCOMPARE(saves,1);
  QVERIFY(parent.grab().save("/tmp/original-settings-narrow.png"));
  parent.resize(800,700);
  QTRY_COMPARE(layer->width(),440);QTRY_VERIFY(layerFits());
  QCOMPARE(field->getLastText(),QString("缩放保留草稿"));
  QVERIFY(inside(save));QVERIFY(inside(cancel));
  QTest::mouseClick(cancel,Qt::LeftButton);
  QTRY_VERIFY(overlay.isNull());QCOMPARE(saves,1);
#endif
 }
 void settingsActionsAndParentDestruction() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Original lib_ui modal host requires FULL mode");
#else
  auto parent=std::make_unique<QWidget>();parent->resize(800,900);parent->show();
  int fonts=0;bool notifications=true;TelegramSettingsActions actions;
  actions.chooseFont=[&]{++fonts;};actions.setNotifications=[&](bool value){notifications=value;};
  actions.versionNotice="仅限可信局域网，未进行身份认证";
  QPointer<TelegramProfileOverlay> overlay=telegramShowProfileBox(parent.get(),{},[](auto){return false;},std::move(actions));
  auto font=overlay->findChild<QWidget*>("telegramChooseFont");QVERIFY(font);
  QTest::mouseClick(font,Qt::LeftButton);QCOMPARE(fonts,1);
  auto checkbox=overlay->findChild<QWidget*>("telegramNotifications");QVERIFY(checkbox);
  QTest::mouseClick(checkbox,Qt::LeftButton);QVERIFY(!notifications);
  parent.reset();QVERIFY(overlay.isNull());
#endif
 }
};
QTEST_MAIN(ProfileBoxTests)
#include "test_telegram_profile_box.moc"
