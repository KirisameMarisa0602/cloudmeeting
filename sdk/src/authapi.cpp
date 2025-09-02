#include "authapi.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace cmsdk {

AuthApi::AuthApi(QObject* parent) : QObject(parent) {}

AuthResult AuthApi::login(const QString& role, const QString& username, const QString& password) {
    AuthResult result;
    
    QJsonObject req {
        {"action", "login"},
        {"role", role},
        {"username", username},
        {"password", password}
    };
    
    QJsonObject reply;
    QString errMsg;
    if (!sendRequest(req, reply, &errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    
    if (!reply.value("ok").toBool()) {
        result.errorMessage = reply.value("msg").toString("账号或密码错误");
        return result;
    }
    
    result.success = true;
    result.username = reply.value("username").toString();
    return result;
}

AuthResult AuthApi::registerUser(const QString& role, const QString& username, const QString& password) {
    AuthResult result;
    
    QJsonObject req {
        {"action", "register"},
        {"role", role},
        {"username", username},
        {"password", password}
    };
    
    QJsonObject reply;
    QString errMsg;
    if (!sendRequest(req, reply, &errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    
    if (!reply.value("ok").toBool()) {
        result.errorMessage = reply.value("msg").toString("注册失败");
        return result;
    }
    
    result.success = true;
    return result;
}

bool AuthApi::sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg) {
    QTcpSocket sock;
    sock.connectToHost(QHostAddress(serverHost_), serverPort_);
    if (!sock.waitForConnected(3000)) {
        if (errMsg) *errMsg = "服务器连接失败";
        return false;
    }
    
    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    if (sock.write(line) == -1 || !sock.waitForBytesWritten(2000)) {
        if (errMsg) *errMsg = "请求发送失败";
        return false;
    }
    
    if (!sock.waitForReadyRead(5000)) {
        if (errMsg) *errMsg = "服务器无响应";
        return false;
    }
    
    QByteArray resp = sock.readAll();
    if (int nl = resp.indexOf('\n'); nl >= 0) resp = resp.left(nl);
    
    QJsonParseError pe{};
    QJsonDocument rdoc = QJsonDocument::fromJson(resp, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (errMsg) *errMsg = "服务器响应格式错误";
        return false;
    }
    
    reply = rdoc.object();
    return true;
}

} // namespace cmsdk