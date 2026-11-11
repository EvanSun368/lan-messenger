#pragma once
#include <QString>
#include <optional>

struct AppVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    QString toString() const;
};

bool operator==(const AppVersion &, const AppVersion &);
bool operator<(const AppVersion &, const AppVersion &);
inline bool operator!=(const AppVersion &a, const AppVersion &b) { return !(a == b); }
inline bool operator>(const AppVersion &a, const AppVersion &b) { return b < a; }
inline bool operator<=(const AppVersion &a, const AppVersion &b) { return !(b < a); }
inline bool operator>=(const AppVersion &a, const AppVersion &b) { return !(a < b); }

// Accepts "0.6.1", "v0.6.1", and leading vMAJOR.MINOR.PATCH in a tag or file name
// such as "v0.6.0-telegram-mail-vpn-windows-x64".
std::optional<AppVersion> parseAppVersion(const QString &text);
