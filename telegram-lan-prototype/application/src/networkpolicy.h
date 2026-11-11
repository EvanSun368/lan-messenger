#pragma once
#include <QHostAddress>
#include <QJsonObject>
#include <QStringList>
#include <QJsonArray>
#include <functional>

// Application address policy only. It neither creates nor verifies a VPN tunnel,
// installs routes/firewall rules, nor authenticates the remote application user.
struct NetworkPolicy {
    bool vpnOnly = false;
    QString interfaceName;
    QHostAddress localAddress;
    QList<QHostAddress> allowedPeers;
    QString mode() const { return vpnOnly ? QStringLiteral("vpn") : QStringLiteral("lan"); }
    static bool privateAddress(const QHostAddress &address);
    static bool vpnAddress(const QHostAddress &address);
    bool allows(const QHostAddress &address) const;
    // Shape and current OS interface/address ownership, checked before every dial.
    QString configurationError() const;
    QString availabilityError() const;
    QJsonObject diagnostics() const;
    static QJsonArray interfaces();
  private:
    // Deterministic test seam, not exposed to command-line/configuration input.
    friend class NetworkPolicyTest;
    std::function<QJsonArray()> m_interfaceSnapshot;
    QJsonArray interfaceSnapshot() const { return m_interfaceSnapshot ? m_interfaceSnapshot() : interfaces(); }
};
