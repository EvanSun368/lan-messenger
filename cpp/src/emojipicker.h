#pragma once
#include <QDialog>
#include <QStringList>
class QListWidget;
class QTabBar;
class EmojiPicker : public QDialog {
    Q_OBJECT
public:
    explicit EmojiPicker(QWidget *parent = nullptr);
    static QList<QPair<QString,QStringList>> categories();
signals:
    void selected(QString emoji);
private:
    QListWidget *m_list;
    QTabBar *m_tabs;
    void populate();
};
