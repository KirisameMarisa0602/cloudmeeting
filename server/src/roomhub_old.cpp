#include "roomhub.h"

RoomHub::RoomHub(QObject* parent) : QObject(parent) {}

bool RoomHub::start(quint16 port) {
    connect(&server_, &QTcpServer::newConnection, this, &RoomHub::onNewConnection);
    if (!server_.listen(QHostAddress::Any, port)) {
        qWarning() << "Listen failed on port" << port << ":" << server_.errorString();
        return false;
    }
    qInfo() << "Server listening on" << server_.serverAddress().toString() << ":" << port;
    return true;
}

void RoomHub::onNewConnection() {
    while (server_.hasPendingConnections()) {
        QTcpSocket* sock = server_.nextPendingConnection();
        auto* ctx = new ClientCtx;
        ctx->sock = sock;
        clients_.insert(sock, ctx);

        qInfo() << "New client from" << sock->peerAddress().toString() << sock->peerPort();

        connect(sock, &QTcpSocket::readyRead, this, &RoomHub::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &RoomHub::onDisconnected);
    }
}

void RoomHub::onDisconnected() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    auto it = clients_.find(sock);
    if (it == clients_.end()) return;
    ClientCtx* c = it.value();

    const QString oldRoom = c->roomId;
    if (!oldRoom.isEmpty()) {
        auto range = rooms_.equal_range(oldRoom);
        for (auto i = range.first; i != range.second; ) {
            if (i.value() == sock) i = rooms_.erase(i);
            else ++i;
        }
        broadcastRoomMembers(oldRoom, "leave", c->user);
    }

    qInfo() << "Client disconnected" << c->user << c->roomId;
    clients_.erase(it);
    sock->deleteLater();
    delete c;
}

void RoomHub::onReadyRead() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    auto it = clients_.find(sock);
    if (it == clients_.end()) return;
    ClientCtx* c = it.value();

    c->buffer.append(sock->readAll());

    QVector<Packet> pkts;
    if (drainPackets(c->buffer, pkts)) {
        for (const Packet& p : pkts) {
            handlePacket(c, p);
        }
    }
}

void RoomHub::handlePacket(ClientCtx* c, const Packet& p) {
    if (p.type == MSG_JOIN_WORKORDER) {
        const QString roomId = p.json.value("roomId").toString();
        const QString user   = p.json.value("user").toString();
        if (roomId.isEmpty()) {
            QJsonObject j{{"code",400},{"message","roomId required"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, j));
            return;
        }
        c->user = user;
        joinRoom(c, roomId);

        // 加入确认
        QJsonObject ack{{"code",0},{"message","joined"},{"roomId",roomId}};
        c->sock->write(buildPacket(MSG_SERVER_EVENT, ack));

        // 1) 单发当前成员列表给新加入者（快照）
        sendRoomMembersTo(c->sock, roomId, "snapshot", c->user);

        // 2) 广播“加入”事件给全房间
        qInfo() << "Join" << roomId << "user" << (user.isEmpty() ? "(anonymous)" : user);
        broadcastRoomMembers(roomId, "join", c->user);
        return;
    }

    if (c->roomId.isEmpty()) {
        QJsonObject j{{"code",403},{"message","join a room first"}};
        c->sock->write(buildPacket(MSG_SERVER_EVENT, j));
        return;
    }

    // 统一转发：文本/设备/视频/音频/控制/标注
    if (p.type == MSG_TEXT ||
        p.type == MSG_DEVICE_DATA ||
        p.type == MSG_VIDEO_FRAME ||
        p.type == MSG_AUDIO_FRAME ||
        p.type == MSG_CONTROL ||
        p.type == MSG_ANNOT)            // 新增：标注消息
    {
        // 标注没有二进制（bin 为空），但用统一打包即可
        QByteArray raw = buildPacket(p.type, p.json, p.bin);
        const bool isVideo = (p.type == MSG_VIDEO_FRAME);
        // 视频帧在对端 backlog 太大时丢弃；其他消息（包含标注）不丢
        broadcastToRoom(c->roomId, raw, c->sock, isVideo);

        if (p.type == MSG_ANNOT) {
            // 简单日志，便于排查
            qInfo() << "[ANNOT] forwarded"
                    << "room="   << c->roomId
                    << "sender=" << p.json.value("sender").toString()
                    << "target=" << p.json.value("target").toString()
                    << "op="     << p.json.value("op").toString();
        }
        return;
    }

    QJsonObject j{{"code",404},{"message",QString("unknown type %1").arg(p.type)}};
    c->sock->write(buildPacket(MSG_SERVER_EVENT, j));
}

void RoomHub::joinRoom(ClientCtx* c, const QString& roomId) {
    if (!c->roomId.isEmpty()) {
        auto range = rooms_.equal_range(c->roomId);
        for (auto i = range.first; i != range.second; ) {
            if (i.value() == c->sock) i = rooms_.erase(i);
            else ++i;
        }
    }
    c->roomId = roomId;
    rooms_.insert(roomId, c->sock);
}

void RoomHub::broadcastToRoom(const QString& roomId,
                              const QByteArray& packet,
                              QTcpSocket* except,
                              bool dropVideoIfBacklog) {
    auto range = rooms_.equal_range(roomId);
    for (auto i = range.first; i != range.second; ++i) {
        QTcpSocket* s = i.value();
        if (s == except) continue;
        if (dropVideoIfBacklog && s->bytesToWrite() > kBacklogDropThreshold) {
            continue; // 丢弃视频帧
        }
        s->write(packet);
    }
}

QStringList RoomHub::listMembers(const QString& roomId) const {
    QStringList members;
    auto range = rooms_.equal_range(roomId);
    for (auto i = range.first; i != range.second; ++i) {
        QTcpSocket* s = i.value();
        if (!clients_.contains(s)) continue;
        auto* c = clients_.value(s);
        if (!c->user.isEmpty()) members << c->user;
        else members << QString("peer-%1").arg(reinterpret_cast<quintptr>(s));
    }
    members.removeDuplicates();
    members.sort();
    return members;
}

void RoomHub::broadcastRoomMembers(const QString& roomId, const QString& event, const QString& whoChanged) {
    QJsonObject j{
        {"code", 0},
        {"kind", "room"},
        {"event", event},          // "join"/"leave"/"snapshot"
        {"roomId", roomId},
        {"who", whoChanged},
        {"members", QJsonArray::fromStringList(listMembers(roomId))},
        {"ts", QDateTime::currentMSecsSinceEpoch()}
    };
    QByteArray pkt = buildPacket(MSG_SERVER_EVENT, j);
    broadcastToRoom(roomId, pkt, nullptr, false);
}

void RoomHub::sendRoomMembersTo(QTcpSocket* target, const QString& roomId, const QString& event, const QString& whoChanged) {
    if (!target) return;
    QJsonObject j{
        {"code", 0},
        {"kind", "room"},
        {"event", event},          // 如 "snapshot"
        {"roomId", roomId},
        {"who", whoChanged},
        {"members", QJsonArray::fromStringList(listMembers(roomId))},
        {"ts", QDateTime::currentMSecsSinceEpoch()}
    };
    target->write(buildPacket(MSG_SERVER_EVENT, j));
}