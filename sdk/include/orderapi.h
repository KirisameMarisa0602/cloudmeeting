#pragma once
#include <QtCore>
#include <QtNetwork>

namespace cmsdk {

struct OrderInfo {
    int id = 0;
    QString title;
    QString desc;
    QString status;
    QString publisher;
    QString accepter;
};

struct OrderResult {
    bool success = false;
    QString errorMessage;
    QVector<OrderInfo> orders; // for getOrders
    int newOrderId = 0;        // for newOrder
};

class OrderApi : public QObject {
    Q_OBJECT
public:
    explicit OrderApi(QObject* parent = nullptr);
    
    // Synchronous order operations (TCP 5555, JSON line protocol) 
    OrderResult getOrders(const QString& role = QString(), 
                         const QString& username = QString(),
                         const QString& status = QString(), 
                         const QString& keyword = QString());
    OrderResult newOrder(const QString& title, const QString& desc, const QString& factoryUser);
    OrderResult updateOrder(int id, const QString& status, const QString& accepter = QString());
    OrderResult deleteOrder(int id, const QString& factoryUser);
    
    // Configuration
    void setServerHost(const QString& host) { serverHost_ = host; }
    void setServerPort(quint16 port) { serverPort_ = port; }
    QString serverHost() const { return serverHost_; }
    quint16 serverPort() const { return serverPort_; }

private:
    bool sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg = nullptr);
    
    QString serverHost_ = "127.0.0.1";
    quint16 serverPort_ = 5555;
};

} // namespace cmsdk