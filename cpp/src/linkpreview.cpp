#include "linkpreview.h"
#include <QDateTime>
#include <QHostInfo>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QSslSocket>
#include <QTextDocument>
#include <QTimer>
#include <memory>
bool LinkPreview::allowedUrl(const QUrl &u) {
    return u.isValid() && (u.scheme() == "http" || u.scheme() == "https") && !u.host().isEmpty() &&
           u.userInfo().isEmpty() && !u.host().endsWith('.') && u.toString().size() < 4096 &&
           (u.port(-1) == -1 || u.port() == 80 || u.port() == 443);
}
bool LinkPreview::publicAddress(const QHostAddress &a) {
    if (a.protocol() == QAbstractSocket::IPv6Protocol) {
        return a.isInSubnet(QHostAddress("2000::"), 3) && !a.isInSubnet(QHostAddress("2001::"), 23) &&
               !a.isInSubnet(QHostAddress("2001:db8::"), 32) && !a.isInSubnet(QHostAddress("2002::"), 16) &&
               !a.isInSubnet(QHostAddress("3fff::"), 20);
    }
    if (a.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    for (auto subnet : {"0.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "127.0.0.0/8", "169.254.0.0/16",
                        "172.16.0.0/12", "192.0.0.0/24", "192.0.2.0/24", "192.168.0.0/16", "198.18.0.0/15",
                        "198.51.100.0/24", "203.0.113.0/24", "224.0.0.0/3"})
        if (a.isInSubnet(QHostAddress::parseSubnet(subnet)))
            return false;
    return true;
}
QPair<QString, QString> LinkPreview::metadata(QString html) {
    auto plain = [](QString s, int max) {
        QTextDocument d;
        d.setHtml(s);
        return d.toPlainText().simplified().left(max);
    };
    QRegularExpression titleRe("<title[^>]*>(.*?)</title>",
                               QRegularExpression::CaseInsensitiveOption |
                                   QRegularExpression::DotMatchesEverythingOption);
    QString title = plain(titleRe.match(html).captured(1), 300), desc;
    QRegularExpression metaRe("<meta\\b[^>]*>", QRegularExpression::CaseInsensitiveOption),
        attrRe("([a-zA-Z_:][-a-zA-Z0-9_:.]*)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))");
    auto tags = metaRe.globalMatch(html);
    while (tags.hasNext()) {
        QHash<QString, QString> attrs;
        auto fields = attrRe.globalMatch(tags.next().captured());
        while (fields.hasNext()) {
            auto m = fields.next();
            attrs[m.captured(1).toLower()] = m.captured(2) + m.captured(3) + m.captured(4);
        }
        auto key = attrs.value("property", attrs.value("name")).toLower();
        if (key == "og:title")
            title = plain(attrs.value("content"), 300);
        if (key == "description" || key == "og:description")
            desc = plain(attrs.value("content"), 600);
    }
    return {title, desc};
}
void LinkPreview::fetch(QUrl u, Callback cb) {
    fetchStep(u, cb, 0, QDateTime::currentMSecsSinceEpoch() + 10000);
}
void LinkPreview::fetchStep(QUrl u, Callback callback, int redirects, qint64 deadline) {
    if (!allowedUrl(u) || redirects > 3) {
        callback({}, {}, QStringLiteral("链接无效或重定向过多"));
        return;
    }
    qint64 remaining = deadline - QDateTime::currentMSecsSinceEpoch();
    if (remaining <= 0) {
        callback({}, {}, QStringLiteral("预览超时"));
        return;
    }
    auto done = std::make_shared<bool>(false);
    auto complete = [done, callback](QString a, QString b, QString c) {
        if (!*done) {
            *done = true;
            callback(a, b, c);
        }
    };
    auto timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->start(int(remaining));
    connect(timer, &QTimer::timeout, this, [complete, timer] {
        complete({}, {}, QStringLiteral("预览超时"));
        timer->deleteLater();
    });
    QHostInfo::lookupHost(
        u.host(), this, [this, u, done, complete, callback, timer, redirects, deadline](QHostInfo info) {
            if (*done)
                return;
            if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
                complete({}, {}, QStringLiteral("域名解析失败"));
                timer->stop();
                timer->deleteLater();
                return;
            }
            for (auto a : info.addresses())
                if (!publicAddress(a)) {
                    complete({}, {}, QStringLiteral("不预览本机、内网或保留地址"));
                    timer->stop();
                    timer->deleteLater();
                    return;
                }
            auto s = new QSslSocket(this);
            s->setProxy(QNetworkProxy::NoProxy);
            s->setReadBufferSize(270000);
            auto data = std::make_shared<QByteArray>();
            auto finish = [complete, timer, s](QString title, QString desc, QString error) {
                complete(title, desc, error);
                timer->stop();
                timer->deleteLater();
                s->abort();
                s->deleteLater();
            };
            connect(timer, &QTimer::timeout, s, [s] {
                s->abort();
                s->deleteLater();
            });
            auto request = [s, u] {
                QByteArray path = u.path(QUrl::FullyEncoded).toUtf8();
                if (path.isEmpty())
                    path = "/";
                if (u.hasQuery())
                    path += '?' + u.query(QUrl::FullyEncoded).toUtf8();
                s->write("GET " + path + " HTTP/1.0\r\nHost: " + QUrl::toAce(u.host()) +
                         "\r\nAccept: text/html\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
            };
            connect(s, &QTcpSocket::connected, s, [s, u, request] {
                if (u.scheme() == "https") {
                    s->setPeerVerifyName(u.host());
                    s->startClientEncryption();
                } else
                    request();
            });
            connect(s, &QSslSocket::encrypted, s, request);
            connect(s, &QTcpSocket::errorOccurred, s, [finish, done](QAbstractSocket::SocketError e) {
                if (!*done && e != QAbstractSocket::RemoteHostClosedError)
                    finish({}, {}, QStringLiteral("网络或证书验证失败"));
            });
            connect(s, &QTcpSocket::readyRead, s, [s, data, finish, done] {
                if (*done)
                    return;
                *data += s->readAll();
                if (data->size() > 262144)
                    finish({}, {}, QStringLiteral("网页超过预览大小限制"));
            });
            connect(s, &QTcpSocket::disconnected, s,
                    [this, u, data, finish, done, s, timer, callback, redirects, deadline] {
                        if (*done)
                            return;
                        int p = data->indexOf("\r\n\r\n");
                        if (p < 0) {
                            finish({}, {}, QStringLiteral("响应格式不支持"));
                            return;
                        }
                        auto lines = data->left(p).split('\n');
                        int status = lines.first().simplified().split(' ').value(1).toInt();
                        QHash<QByteArray, QByteArray> headers;
                        for (int i = 1; i < lines.size(); ++i) {
                            int c = lines[i].indexOf(':');
                            if (c > 0)
                                headers[lines[i].left(c).trimmed().toLower()] = lines[i].mid(c + 1).trimmed();
                        }
                        if (status == 301 || status == 302 || status == 303 || status == 307 ||
                            status == 308) {
                            QUrl next = u.resolved(QUrl(QString::fromUtf8(headers.value("location"))));
                            if (headers.value("location").isEmpty()) {
                                finish({}, {}, "无效重定向");
                                return;
                            }
                            *done = true;
                            timer->stop();
                            timer->deleteLater();
                            s->deleteLater();
                            fetchStep(next, callback, redirects + 1, deadline);
                            return;
                        }
                        if (status != 200) {
                            finish({}, {}, QStringLiteral("网页状态不支持"));
                            return;
                        }
                        if (!headers.value("content-type").toLower().startsWith("text/html") ||
                            (headers.contains("content-encoding") &&
                             headers.value("content-encoding").toLower() != "identity")) {
                            finish({}, {}, QStringLiteral("网页格式不支持"));
                            return;
                        }
                        QByteArray body = data->mid(p + 4);
                        if (headers.contains("transfer-encoding")) {
                            if (headers.value("transfer-encoding").toLower() != "chunked") {
                                finish({}, {}, "传输格式不支持");
                                return;
                            }
                            QByteArray decoded;
                            int pos = 0;
                            while (true) {
                                int end = body.indexOf("\r\n", pos);
                                if (end < 0) {
                                    finish({}, {}, "分块响应无效");
                                    return;
                                }
                                bool ok = false;
                                auto n = body.mid(pos, end - pos).split(';').first().toULongLong(&ok, 16);
                                pos = end + 2;
                                if (!ok || n > 262144 || pos + qint64(n) + 2 > body.size()) {
                                    finish({}, {}, "分块响应无效");
                                    return;
                                }
                                if (n == 0)
                                    break;
                                if (body.mid(pos + n, 2) != "\r\n") {
                                    finish({}, {}, "分块响应无效");
                                    return;
                                }
                                decoded += body.mid(pos, n);
                                pos += int(n) + 2;
                            }
                            body = decoded;
                        }
                        auto m = metadata(QString::fromUtf8(body));
                        finish(m.first, m.second, {});
                    });
            // Pin one validated address; TLS still verifies the original DNS name.
            s->connectToHost(info.addresses().first(), u.port(u.scheme() == "https" ? 443 : 80));
        });
}
