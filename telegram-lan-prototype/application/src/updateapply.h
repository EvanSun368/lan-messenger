#pragma once
#include <QString>

QString updateStagingRoot(const QString &storage);
QString githubReleasesPageUrl();
QString githubReleasesApiUrl();
bool startApplyAfterExit(qint64 pid, const QString &executable, const QString &storage, QString *error = nullptr);
