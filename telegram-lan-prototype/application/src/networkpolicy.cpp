#include "networkpolicy.h"
#include <QJsonArray>
#include <QNetworkInterface>

bool NetworkPolicy::privateAddress(const QHostAddress &a) {
    if (a.isLoopback()) return true;
    return a.isInSubnet(QHostAddress("10.0.0.0"), 8) || a.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
           a.isInSubnet(QHostAddress("192.168.0.0"), 16) || a.isInSubnet(QHostAddress("169.254.0.0"), 16) ||
           a.isInSubnet(QHostAddress("fc00::"), 7) || a.isInSubnet(QHostAddress("fe80::"), 10);
}
bool NetworkPolicy::vpnAddress(const QHostAddress &a) {
    return a.protocol() == QAbstractSocket::IPv4Protocol &&
        (privateAddress(a) || a.isInSubnet(QHostAddress("100.64.0.0"), 10));
}
bool NetworkPolicy::allows(const QHostAddress &a) const {
    if (!vpnOnly) return privateAddress(a);
    return vpnAddress(a) && allowedPeers.contains(a);
}
QString NetworkPolicy::configurationError() const {
    if (!vpnOnly) {
        if (!interfaceName.isEmpty() || !localAddress.isNull() || !allowedPeers.isEmpty())
            return QStringLiteral("VPN 参数必须与 VPN 专用模式一同启用");
        return {};
    }
    if (interfaceName.trimmed().isEmpty() || !vpnAddress(localAddress))
        return QStringLiteral("请指定 VPN 接口的系统名称及本机私网/100.64.0.0/10 IPv4 地址");
    if (allowedPeers.isEmpty() || allowedPeers.size() > 256)
        return QStringLiteral("VPN 模式需要 1–256 个明确允许的对端 IPv4 地址");
    for (const auto &peer : allowedPeers)
        if (!vpnAddress(peer)) return QStringLiteral("VPN 对端仅接受精确私网或 100.64.0.0/10 IPv4，不接受公网、域名或网段");
    return {};
}
QString NetworkPolicy::availabilityError() const {
    const auto invalid = configurationError();
    if (!invalid.isEmpty() || !vpnOnly) return invalid;
    for (const auto &entry : interfaceSnapshot()) {
        const auto iface = entry.toObject();
        if (iface["name"].toString() != interfaceName) continue;
        if (!iface["up"].toBool() || !iface["running"].toBool())
            return QStringLiteral("指定的 VPN 接口未运行；不会回退至局域网");
        for (const auto &address : iface["ipv4"].toArray())
            if (QHostAddress(address.toString()) == localAddress) return {};
        return QStringLiteral("指定 IPv4 不属于当前 VPN 接口；不会回退至其他地址");
    }
    return QStringLiteral("指定的 VPN 接口不存在或无法枚举；不会回退至局域网");
}
QJsonArray NetworkPolicy::interfaces() {
    QJsonArray result;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        QJsonArray addresses;
        for (const auto &entry : iface.addressEntries())
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) addresses.append(entry.ip().toString());
        result.append(QJsonObject{{"name",iface.name()},{"label",iface.humanReadableName()},
            {"up",iface.flags().testFlag(QNetworkInterface::IsUp)},
            {"running",iface.flags().testFlag(QNetworkInterface::IsRunning)},{"ipv4",addresses}});
    }
    return result;
}
QJsonObject NetworkPolicy::diagnostics() const {
    QJsonArray peers;
    for (const auto &address : allowedPeers) peers.append(address.toString());
    return {{"mode",mode()},{"interface",interfaceName},
        {"bind_address",vpnOnly?localAddress.toString():QStringLiteral("0.0.0.0")},
        {"allowed_peers",peers},{"availability_error",availabilityError()},
        {"application_identity_verified",false},{"application_encryption",false},
        {"vpn_tunnel_verified",false},{"interfaces",interfaceSnapshot()}};
}
