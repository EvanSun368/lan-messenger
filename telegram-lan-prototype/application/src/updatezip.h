#pragma once
#include <QHash>
#include <QString>
#include <QByteArray>

bool extractZip(const QString &zipPath, const QString &destination, QString *error = nullptr);
bool writeZip(const QString &zipPath, const QHash<QString, QByteArray> &files, QString *error = nullptr);
QString sha256File(const QString &path, QString *error = nullptr);
QString sha256Bytes(const QByteArray &bytes);
