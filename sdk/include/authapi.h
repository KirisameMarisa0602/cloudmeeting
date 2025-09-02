#pragma once
#include <QtCore>
#include <QtNetwork>

namespace cmsdk {

struct AuthResult {
    bool success = false;
    QString errorMessage;
    QString username; // populated on successful login
};

class AuthApi : public QObject {
    Q_OBJECT
public:
    explicit AuthApi(QObject* parent = nullptr);
    
    // Synchronous auth operations (TCP 5555, JSON line protocol)
    AuthResult login(const QString& role, const QString& username, const QString& password);
    AuthResult registerUser(const QString& role, const QString& username, const QString& password);
    
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