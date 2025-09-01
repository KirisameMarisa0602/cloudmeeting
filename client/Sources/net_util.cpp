#include "net_util.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>

bool sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg)
{
    QTcpSocket sock;
    sock.connectToHost(QHostAddress(QString::fromLatin1(serverHost())), serverPort());
    if (!sock.waitForConnected(3000)) { if (errMsg) *errMsg = "服务器连接失败"; return false; }

    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    if (sock.write(line) == -1 || !sock.waitForBytesWritten(2000)) { if (errMsg) *errMsg = "请求发送失败"; return false; }

    if (!sock.waitForReadyRead(5000)) { if (errMsg) *errMsg = "服务器无响应"; return false; }

    QByteArray resp = sock.readAll();
    if (int nl = resp.indexOf('\n'); nl >= 0) resp = resp.left(nl);

    QJsonParseError pe{};
    QJsonDocument rdoc = QJsonDocument::fromJson(resp, &pe);
    if (pe.error != QJsonParseError::NoError || !rdoc.isObject()) { if (errMsg) *errMsg = "响应解析失败"; return false; }

    reply = rdoc.object();
    return true;
}
