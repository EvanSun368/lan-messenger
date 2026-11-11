#pragma once
#include <QHash>
#include <QString>
// Private, per-installation address book. Never part of Node discovery/profile.
class ContactMailBook {
public:
 explicit ContactMailBook(QString path):path_(std::move(path)){}
 bool load(QString *error=nullptr);
 QString email(const QString &peerId) const {return entries_.value(peerId);}
 bool setEmail(const QString &peerId,QString email,QString *error=nullptr);
 static bool validEmail(const QString &email);
private:
 QString path_;
 QHash<QString,QString> entries_;
};
