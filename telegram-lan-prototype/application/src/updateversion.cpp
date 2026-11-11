#include "updateversion.h"
#include <QRegularExpression>

QString AppVersion::toString() const {
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

bool operator==(const AppVersion &a, const AppVersion &b) {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

bool operator<(const AppVersion &a, const AppVersion &b) {
    if (a.major != b.major) return a.major < b.major;
    if (a.minor != b.minor) return a.minor < b.minor;
    return a.patch < b.patch;
}

std::optional<AppVersion> parseAppVersion(const QString &text) {
    static const QRegularExpression pattern(
        QStringLiteral(R"((?i)(?<![\d.])v?([0-9]{1,6})\.([0-9]{1,6})\.([0-9]{1,6})(?![A-Za-z0-9.]))"));
    const auto match = pattern.match(text);
    if (!match.hasMatch()) return std::nullopt;
    AppVersion version;
    version.major = match.captured(1).toInt();
    version.minor = match.captured(2).toInt();
    version.patch = match.captured(3).toInt();
    return version;
}
