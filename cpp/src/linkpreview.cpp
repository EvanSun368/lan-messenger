#include "linkpreview.h"
#include <QHostInfo>
#include <QSslSocket>
#include <QNetworkProxy>
#include <QTimer>
#include <QRegularExpression>
#include <QTextDocument>
#include <memory>
bool LinkPreview::allowedUrl(const QUrl &u){return u.isValid() && (u.scheme()=="http"||u.scheme()=="https") && !u.host().isEmpty() && u.userInfo().isEmpty() && !u.host().endsWith('.') && u.toString().size()<4096 && (u.port(-1)==-1||u.port()==80||u.port()==443);}
bool LinkPreview::publicAddress(const QHostAddress &a){
 if(a.protocol()!=QAbstractSocket::IPv4Protocol)return false; // conservatively reject IPv6 transition/local forms
 auto v=a.toIPv4Address();auto first=v>>24;
 return first!=0&&first!=10&&first!=127&&first<224&&(v>>16)!=0xa9fe&&(v>>16)!=0xc0a8&&(v>>20)!=0xac1&&(v&0xffc00000)!=0x64400000&&(v>>24)!=192&&(v>>24)!=198&&(v>>24)!=203;
}
void LinkPreview::fetch(QUrl u,Callback callback){
 if(!allowedUrl(u)){callback({}, {}, QStringLiteral("链接无效或使用非标准端口"));return;}
 auto done=std::make_shared<bool>(false);auto complete=[done,callback](QString a,QString b,QString c){if(!*done){*done=true;callback(a,b,c);}};
 auto timer=new QTimer(this);timer->setSingleShot(true);timer->start(8000);
 connect(timer,&QTimer::timeout,this,[complete,timer]{complete({}, {},QStringLiteral("预览超时"));timer->deleteLater();});
 QHostInfo::lookupHost(u.host(),this,[this,u,done,complete,timer](QHostInfo info){
 if(*done)return;
 if(info.error()!=QHostInfo::NoError||info.addresses().isEmpty()){complete({}, {},QStringLiteral("域名解析失败"));timer->stop();timer->deleteLater();return;}
 for(auto a:info.addresses())if(!publicAddress(a)){complete({}, {},QStringLiteral("不预览本机、内网或保留地址"));timer->stop();timer->deleteLater();return;}
 auto s=new QSslSocket(this);s->setProxy(QNetworkProxy::NoProxy);s->setReadBufferSize(270000);auto data=std::make_shared<QByteArray>();
 auto finish=[complete,timer,s](QString title,QString desc,QString error){complete(title,desc,error);timer->stop();timer->deleteLater();s->abort();s->deleteLater();};
 connect(timer,&QTimer::timeout,s,[s]{s->abort();s->deleteLater();});
 auto request=[s,u]{QByteArray path=u.path(QUrl::FullyEncoded).toUtf8();if(path.isEmpty())path="/";if(u.hasQuery())path+='?'+u.query(QUrl::FullyEncoded).toUtf8();s->write("GET "+path+" HTTP/1.0\r\nHost: "+u.host().toUtf8()+"\r\nAccept: text/html\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");};
 connect(s,&QTcpSocket::connected,s,[s,u,request]{if(u.scheme()=="https"){s->setPeerVerifyName(u.host());s->startClientEncryption();}else request();});
 connect(s,&QSslSocket::encrypted,s,request);
 connect(s,&QTcpSocket::errorOccurred,s,[finish,done](QAbstractSocket::SocketError e){if(!*done&&e!=QAbstractSocket::RemoteHostClosedError)finish({}, {},QStringLiteral("网络或证书验证失败"));});
 connect(s,&QTcpSocket::readyRead,s,[s,data,finish,done]{if(*done)return;*data+=s->readAll();if(data->size()>262144)finish({}, {},QStringLiteral("网页超过预览大小限制"));});
 connect(s,&QTcpSocket::disconnected,s,[data,finish,done]{if(*done)return;int p=data->indexOf("\r\n\r\n");if(p<0){finish({}, {},QStringLiteral("响应格式不支持"));return;}auto head=data->left(p).toLower();if(!head.startsWith("http/1.0 200")&&!head.startsWith("http/1.1 200")){finish({}, {},QStringLiteral("网页状态或重定向不支持"));return;}if(!head.contains("content-type: text/html")||head.contains("transfer-encoding:")||head.contains("content-encoding:")){finish({}, {},QStringLiteral("网页格式不支持"));return;}QString body=QString::fromUtf8(data->mid(p+4));QRegularExpression re("<title[^>]*>(.*?)</title>",QRegularExpression::CaseInsensitiveOption|QRegularExpression::DotMatchesEverythingOption);QString title=re.match(body).captured(1);QTextDocument doc;doc.setHtml(title);finish(doc.toPlainText().left(300),{},{});});
 s->connectToHost(info.addresses().first(),u.port(u.scheme()=="https"?443:80));
 });
}
