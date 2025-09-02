#pragma once
#include <QtCore>
#include <QtNetwork>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QUuid>
#include <QTimer>
#include <QDir>
#include "protocol.h"

struct User {
    QString username;
    QString passwordHash;
    QString salt;
    QString role; // "factory" or "expert"
    QString createdAt;
};

struct WorkOrder {
    QString id;
    QString title;
    QString description;
    QString status; // "open", "assigned", "in_progress", "closed", "canceled"
    QString createdBy;
    QString assignedTo;
    QString createdAt;
    QString updatedAt;
};

struct ClientCtx {
    QTcpSocket* sock = nullptr;
    QString user;
    QString roomId;
    QString token;
    QString role; // "factory" or "expert"
    QByteArray buffer;
    qint64 lastSeen = 0;
};

class RoomHub : public QObject {
    Q_OBJECT
public:
    explicit RoomHub(QObject* parent=nullptr);
    bool start(quint16 port);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onHeartbeatTimer();

private:
    QTcpServer server_;
    QHash<QTcpSocket*, ClientCtx*> clients_;
    QMultiHash<QString, QTcpSocket*> rooms_; // roomId -> sockets
    QHash<QString, QString> sessions_; // token -> username
    QTimer* heartbeatTimer_;

    static constexpr qint64 kBacklogDropThreshold = 3 * 1024 * 1024; // 3MB
    static constexpr qint64 kHeartbeatInterval = 30000; // 30 seconds
    static constexpr qint64 kSessionTimeout = 300000; // 5 minutes

    // Message handlers
    void handlePacket(ClientCtx* c, const Packet& p);
    void handleAuth(ClientCtx* c, const Packet& p);
    void handleOrder(ClientCtx* c, const Packet& p);
    void handlePing(ClientCtx* c, const Packet& p);
    
    // Room management
    void joinRoom(ClientCtx* c, const QString& roomId);
    void broadcastToRoom(const QString& roomId,
                         const QByteArray& packet,
                         QTcpSocket* except = nullptr,
                         bool dropVideoIfBacklog = false);

    QStringList listMembers(const QString& roomId) const;
    void broadcastRoomMembers(const QString& roomId, const QString& event, const QString& whoChanged);
    void sendRoomMembersTo(QTcpSocket* target, const QString& roomId, const QString& event, const QString& whoChanged);

    // Authentication and persistence
    bool loadUsers();
    bool saveUsers();
    bool loadWorkOrders();
    bool saveWorkOrders();
    
    QString generateToken();
    QString hashPassword(const QString& password, const QString& salt);
    QString generateSalt();
    bool authenticateUser(const QString& username, const QString& password, const QString& role, QString* outToken);
    bool isValidToken(const QString& token, QString* outUsername = nullptr, QString* outRole = nullptr);
    
    // Work order operations
    QString createWorkOrder(const QString& title, const QString& description, const QString& createdBy);
    QJsonArray getWorkOrdersForUser(const QString& username, const QString& role);
    bool acceptWorkOrder(const QString& orderId, const QString& expertUsername);
    bool updateWorkOrderStatus(const QString& orderId, const QString& newStatus, const QString& username);
    WorkOrder* findWorkOrder(const QString& orderId);
    void broadcastWorkOrderEvent(const QString& event, const WorkOrder& order);

    // Data
    QHash<QString, User> users_; // username -> User
    QHash<QString, WorkOrder> workOrders_; // orderId -> WorkOrder
    QString dataPath_;
};
