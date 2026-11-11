#pragma once
#include <QHash>
#include <QList>
#include <QString>

// Inbox cursors only: never stores headers, bodies, credentials or addresses.
// Account IDs must change when the server/user identity changes.
class MailSyncState {
public:
 struct Result { bool ok=false,baseline=false,reset=false; QList<quint64> newUids; QString error; };
 explicit MailSyncState(QString path={});
 bool load(QString *error=nullptr);
 bool save(QString *error=nullptr) const;
 Result apply(const QString &accountId,const QString &uidValidity,const QList<quint64> &uids);
 QString validity(const QString &accountId) const;
 void remove(const QString &accountId);
 static constexpr int MaxAccounts=1024,MaxUids=100000;
private:
 struct Cursor {QString validity;quint64 highwater=0;};
 QString path_;QHash<QString,Cursor> cursors_;
};
