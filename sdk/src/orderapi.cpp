#include "orderapi.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

namespace cmsdk {

OrderApi::OrderApi(QObject* parent) : QObject(parent) {}

static OrderInfo orderFromJson(const QJsonObject& o) {
    OrderInfo info;
    info.id = o.value("id").toInt();
    info.title = o.value("title").toString();
    info.desc = o.value("desc").toString();
    info.status = o.value("status").toString();
    info.publisher = o.value("publisher").toString();
    info.accepter = o.value("accepter").toString();
    return info;
}

OrderResult OrderApi::getOrders(const QString& role, const QString& username, 
                                const QString& status, const QString& keyword) {
    OrderResult result;
    
    QJsonObject req{{"action", "get_orders"}};
    if (!role.isEmpty()) req["role"] = role;
    if (!username.isEmpty()) req["username"] = username;
    if (!status.isEmpty()) req["status"] = status;
    if (!keyword.isEmpty()) req["keyword"] = keyword;
    
    QJsonObject reply;
    QString errMsg;
    if (!sendRequest(req, reply, &errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    
    if (!reply.value("ok").toBool()) {
        result.errorMessage = reply.value("msg").toString("获取工单失败");
        return result;
    }
    
    const QJsonArray arr = reply.value("orders").toArray();
    result.orders.reserve(arr.size());
    for (const auto& v : arr) {
        result.orders.push_back(orderFromJson(v.toObject()));
    }
    
    result.success = true;
    return result;
}

OrderResult OrderApi::newOrder(const QString& title, const QString& desc, const QString& factoryUser) {
    OrderResult result;
    
    QJsonObject req {
        {"action", "new_order"},
        {"title", title},
        {"desc", desc},
        {"factory_user", factoryUser}
    };
    
    QJsonObject reply;
    QString errMsg;
    if (!sendRequest(req, reply, &errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    
    if (!reply.value("ok").toBool()) {
        result.errorMessage = reply.value("msg").toString("创建工单失败");
        return result;
    }
    
    result.success = true;
    result.newOrderId = reply.value("id").toInt();
    return result;
}

OrderResult OrderApi::updateOrder(int id, const QString& status, const QString& accepter) {
    OrderResult result;
    
    QJsonObject req {
        {"action", "update_order"},
        {"id", id},
        {"status", status}
    };
    if (!accepter.isEmpty()) {
        req["accepter"] = accepter;
    }
    
    QJsonObject reply;
    QString errMsg;
    if (!sendRequest(req, reply, &errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    
    if (!reply.value("ok").toBool()) {
        result.errorMessage = reply.value("msg").toString("更新工单失败");
        return result;
    }
    
    result.success = true;
    return result;
}

OrderResult OrderApi::deleteOrder(int id, const QString& factoryUser) {
    OrderResult result;
    
    QJsonObject req {
        {"action", "delete_order"},
        {"id", id},
        {"factory_user", factoryUser}
    };
    
    QJsonObject reply;
    QString errMsg;
    if (!sendRequest(req, reply, &errMsg)) {
        result.errorMessage = errMsg;
        return result;
    }
    
    if (!reply.value("ok").toBool()) {
        result.errorMessage = reply.value("msg").toString("删除工单失败");
        return result;
    }
    
    result.success = true;
    return result;
}

bool OrderApi::sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg) {
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