#include "updatezip.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QScopeGuard>
#include "miniz.h"
#include "miniz_zip.h"

namespace {
constexpr int MaxZipFiles = 1024;
constexpr qint64 MaxUncompressed = 512ll * 1024 * 1024;

bool fail(QString *error, const QString &text) {
    if (error) *error = text;
    return false;
}

QString relativePath(const QString &name) {
    auto path = QString(name).replace('\\', '/');
    while (path.startsWith('/')) path.remove(0, 1);
    return path;
}

bool isAbsoluteZipPath(const QString &name) {
    const auto path = QString(name).replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.startsWith('/')) return true;
    return path.size() >= 2 && path[0].isLetter() && path[1] == QLatin1Char(':');
}

bool acceptablePath(const QString &name) {
    if (isAbsoluteZipPath(name)) return false;
    const auto path = relativePath(name);
    if (path.isEmpty() || path.size() > 400) return false;
    const auto parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;
    for (const auto &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String("..")) return false;
        if (part.size() > 200) return false;
    }
    return true;
}

QByteArray utf8(const QString &path) { return QFile::encodeName(path); }
}

QString sha256Bytes(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QString sha256File(const QString &path, QString *error) {
    if (error) error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("无法读取下载的更新包")), QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return fail(error, QStringLiteral("无法计算更新包校验和")), QString();
    return QString::fromLatin1(hash.result().toHex());
}

bool extractZip(const QString &zipPath, const QString &destination, QString *error) {
    if (error) error->clear();
    if (zipPath.isEmpty() || destination.isEmpty()) return fail(error, QStringLiteral("解压路径无效"));
    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("无法打开更新包"));
    const auto bytes = file.readAll();
    file.close();
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.constData(), bytes.size(), 0))
        return fail(error, QStringLiteral("无法打开更新包"));
    const auto close = qScopeGuard([&] { mz_zip_reader_end(&zip); });
    const auto files = int(mz_zip_reader_get_num_files(&zip));
    if (files <= 0 || files > MaxZipFiles) return fail(error, QStringLiteral("更新包文件数量无效"));
    QDir dest(destination);
    if (!dest.mkpath(QStringLiteral("."))) return fail(error, QStringLiteral("无法创建解压目录"));
    for (int i = 0; i < files; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) return fail(error, QStringLiteral("无法读取更新包目录"));
        if (stat.m_is_directory) continue;
        if (stat.m_is_encrypted) return fail(error, QStringLiteral("不支持加密的更新包"));
        const auto name = QString::fromUtf8(stat.m_filename);
        if (!acceptablePath(name)) return fail(error, QStringLiteral("更新包包含非法路径"));
        if (qint64(stat.m_uncomp_size) < 0 || qint64(stat.m_uncomp_size) > MaxUncompressed)
            return fail(error, QStringLiteral("更新包文件过大"));
        const auto relative = relativePath(name);
        const auto outPath = dest.filePath(relative);
        if (!QDir().mkpath(QFileInfo(outPath).path())) return fail(error, QStringLiteral("无法创建解压子目录"));
        if (!mz_zip_reader_extract_to_file(&zip, i, utf8(outPath).constData(), 0))
            return fail(error, QStringLiteral("解压更新包失败"));
    }
    return true;
}

bool writeZip(const QString &zipPath, const QHash<QString, QByteArray> &files, QString *error) {
    if (error) error->clear();
    if (zipPath.isEmpty() || files.isEmpty() || files.size() > MaxZipFiles)
        return fail(error, QStringLiteral("无法创建测试更新包"));
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, utf8(zipPath).constData(), 0))
        return fail(error, QStringLiteral("无法写入更新包"));
    bool ok = true;
    for (auto it = files.begin(); it != files.end() && ok; ++it) {
        if (!acceptablePath(it.key())) { ok = false; break; }
        const auto name = relativePath(it.key()).toUtf8();
        ok = mz_zip_writer_add_mem(&zip, name.constData(), it.value().constData(), it.value().size(), MZ_DEFAULT_COMPRESSION);
    }
    if (!ok || !mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        return fail(error, QStringLiteral("写入更新包失败"));
    }
    mz_zip_writer_end(&zip);
    return true;
}
