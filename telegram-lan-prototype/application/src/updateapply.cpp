#include "updateapply.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>

QString updateStagingRoot(const QString &storage) {
    return QDir(storage).filePath(QStringLiteral("updates"));
}

QString githubReleasesPageUrl() {
    return QStringLiteral("https://github.com/EvanSun368/lan-messenger/releases");
}

QString githubReleasesApiUrl() {
    return QStringLiteral("https://api.github.com/repos/EvanSun368/lan-messenger/releases?per_page=30");
}

namespace {
bool fail(QString *error, const QString &text) {
    if (error) *error = text;
    return false;
}

#ifdef Q_OS_WIN
QByteArray helperScript() {
    return QByteArrayLiteral(
        "@echo off\r\n"
        "set PID=%~1\r\n"
        "set EXE=%~2\r\n"
        "set STORE=%~3\r\n"
        ":wait\r\n"
        "tasklist /FI \"PID eq %PID%\" | findstr /R /C:\" %PID% \" >nul\r\n"
        "if not errorlevel 1 (\r\n"
        "  timeout /t 1 /nobreak >nul\r\n"
        "  goto wait\r\n"
        ")\r\n"
        "\"%EXE%\" --apply-update-and-exit --storage \"%STORE%\"\r\n");
}
QString helperName() { return QStringLiteral("apply-on-quit.cmd"); }
QString helperProgram() { return QStringLiteral("cmd.exe"); }
QStringList helperPrefix() { return {QStringLiteral("/c")}; }
#else
QByteArray helperScript() {
    return QByteArrayLiteral(
        "#!/bin/sh\n"
        "pid=$1\n"
        "exe=$2\n"
        "store=$3\n"
        "while kill -0 \"$pid\" 2>/dev/null; do sleep 1; done\n"
        "exec \"$exe\" --apply-update-and-exit --storage \"$store\"\n");
}
QString helperName() { return QStringLiteral("apply-on-quit.sh"); }
QString helperProgram() { return QStringLiteral("/bin/sh"); }
QStringList helperPrefix() { return {}; }
#endif
}

bool startApplyAfterExit(qint64 pid, const QString &executable, const QString &storage, QString *error) {
    if (error) error->clear();
    if (pid <= 0 || executable.isEmpty() || storage.isEmpty())
        return fail(error, QStringLiteral("无法安排退出后安装"));
    const auto script = QDir(updateStagingRoot(storage)).filePath(helperName());
    if (!QDir().mkpath(QFileInfo(script).path())) return fail(error, QStringLiteral("无法写入安装脚本"));
    const auto body = helperScript();
    QSaveFile file(script);
    if (!file.open(QIODevice::WriteOnly) || file.write(body) != body.size() || !file.commit())
        return fail(error, QStringLiteral("无法写入安装脚本"));
    QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                       | QFileDevice::ReadGroup | QFileDevice::ExeGroup);
    QStringList args = helperPrefix();
    args << script << QString::number(pid) << executable << storage;
    return QProcess::startDetached(helperProgram(), args) || fail(error, QStringLiteral("无法启动退出后安装"));
}
