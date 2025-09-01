#pragma once
#include <QtCore>
#include <QtNetwork>
#include "protocol.h"

struct ClientCtx {
    QTcpSocket* sock = nullptr;
    QString user;
    QString roomId;
    QByteArray buffer;
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

private:
    QTcpServer server_;
    QHash<QTcpSocket*, ClientCtx*> clients_;
    QMultiHash<QString, QTcpSocket*> rooms_; // roomId -> sockets

    static constexpr qint64 kBacklogDropThreshold = 3 * 1024 * 1024; // 3MB

    void handlePacket(ClientCtx* c, const Packet& p);
    void joinRoom(ClientCtx* c, const QString& roomId);
    void broadcastToRoom(const QString& roomId,
                         const QByteArray& packet,
                         QTcpSocket* except = nullptr,
                         bool dropVideoIfBacklog = false);

    QStringList listMembers(const QString& roomId) const;
    void broadcastRoomMembers(const QString& roomId, const QString& event, const QString& whoChanged);

    // 新增：给指定 socket 发送当前成员列表（用于刚加入的人）
    void sendRoomMembersTo(QTcpSocket* target, const QString& roomId, const QString& event, const QString& whoChanged);
};
