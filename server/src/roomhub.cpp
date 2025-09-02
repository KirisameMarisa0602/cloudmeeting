#include "roomhub.h"
#include <QCoreApplication>
#include <QStandardPaths>

RoomHub::RoomHub(QObject* parent) : QObject(parent) {
    dataPath_ = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("data");
    
    // Initialize heartbeat timer
    heartbeatTimer_ = new QTimer(this);
    heartbeatTimer_->setInterval(kHeartbeatInterval);
    connect(heartbeatTimer_, &QTimer::timeout, this, &RoomHub::onHeartbeatTimer);
    
    // Load persistent data
    loadUsers();
    loadWorkOrders();
}

bool RoomHub::start(quint16 port) {
    connect(&server_, &QTcpServer::newConnection, this, &RoomHub::onNewConnection);
    if (!server_.listen(QHostAddress::Any, port)) {
        qWarning() << "Listen failed on port" << port << ":" << server_.errorString();
        return false;
    }
    
    // Start heartbeat timer
    heartbeatTimer_->start();
    
    qInfo() << "Server listening on" << server_.serverAddress().toString() << ":" << port;
    return true;
}

void RoomHub::onNewConnection() {
    while (server_.hasPendingConnections()) {
        QTcpSocket* sock = server_.nextPendingConnection();
        auto* ctx = new ClientCtx;
        ctx->sock = sock;
        ctx->lastSeen = QDateTime::currentMSecsSinceEpoch();
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

    // Remove session if exists
    if (!c->token.isEmpty()) {
        sessions_.remove(c->token);
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
    c->lastSeen = QDateTime::currentMSecsSinceEpoch();

    QVector<Packet> pkts;
    if (drainPackets(c->buffer, pkts)) {
        for (const Packet& p : pkts) {
            handlePacket(c, p);
        }
    }
}

void RoomHub::onHeartbeatTimer() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QTcpSocket*> toDisconnect;
    
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        ClientCtx* c = it.value();
        if (now - c->lastSeen > kSessionTimeout) {
            toDisconnect.append(it.key());
        }
    }
    
    // Disconnect timed out clients
    for (QTcpSocket* sock : toDisconnect) {
        qInfo() << "Disconnecting client due to timeout";
        sock->disconnectFromHost();
    }
}

void RoomHub::handlePacket(ClientCtx* c, const Packet& p) {
    // Handle authentication messages without requiring login
    if (p.type == MSG_AUTH) {
        handleAuth(c, p);
        return;
    }
    
    // Handle ping without requiring login
    if (p.type == MSG_PING) {
        handlePing(c, p);
        return;
    }
    
    // For all other messages, require authentication
    if (c->token.isEmpty() || !isValidToken(c->token)) {
        QJsonObject error{{"code", 401}, {"message", "Authentication required"}};
        c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
        return;
    }
    
    switch (p.type) {
        case MSG_ORDER:
            handleOrder(c, p);
            break;
            
        case MSG_JOIN_WORKORDER: {
            const QString roomId = p.json.value("roomId").toString();
            if (roomId.isEmpty()) {
                QJsonObject error{{"code", 400}, {"message", "roomId required"}};
                c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
                return;
            }
            
            // Validate room access permissions for work orders
            WorkOrder* order = findWorkOrder(roomId);
            if (order) {
                bool canJoin = false;
                if (c->role == "factory" && order->createdBy == c->user) {
                    canJoin = true;
                } else if (c->role == "expert" && order->assignedTo == c->user) {
                    canJoin = true;
                }
                
                if (!canJoin) {
                    QJsonObject error{{"code", 403}, {"message", "Not authorized to join this room"}};
                    c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
                    return;
                }
                
                // Update order status to in_progress if both parties join
                if (order->status == "assigned") {
                    order->status = "in_progress";
                    order->updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
                    saveWorkOrders();
                    broadcastWorkOrderEvent("updated", *order);
                }
            }
            
            joinRoom(c, roomId);
            
            QJsonObject ack{{"code", 0}, {"message", "joined"}, {"roomId", roomId}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, ack));
            
            sendRoomMembersTo(c->sock, roomId, "snapshot", c->user);
            broadcastRoomMembers(roomId, "join", c->user);
            break;
        }
        
        default:
            // Handle other message types (forward to room if in room)
            if (!c->roomId.isEmpty()) {
                // Add sender info and forward to room
                QJsonObject json = p.json;
                json["sender"] = c->user;
                json["roomId"] = c->roomId;
                json["ts"] = QDateTime::currentMSecsSinceEpoch();
                
                QByteArray packet = buildPacket(p.type, json, p.bin);
                broadcastToRoom(c->roomId, packet, c->sock, p.type == MSG_VIDEO_FRAME);
            }
            break;
    }
}

// Authentication handlers
void RoomHub::handleAuth(ClientCtx* c, const Packet& p) {
    const QString op = p.json.value("op").toString();
    
    if (op == "register") {
        const QString username = p.json.value("username").toString().trimmed();
        const QString password = p.json.value("password").toString();
        const QString role = p.json.value("role").toString();
        
        if (username.isEmpty() || password.isEmpty()) {
            QJsonObject error{{"kind", "auth"}, {"event", "error"}, {"message", "Username and password required"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
            return;
        }
        
        if (role != "factory" && role != "expert") {
            QJsonObject error{{"kind", "auth"}, {"event", "error"}, {"message", "Role must be 'factory' or 'expert'"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
            return;
        }
        
        if (users_.contains(username)) {
            QJsonObject error{{"kind", "auth"}, {"event", "error"}, {"message", "Username already exists"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
            return;
        }
        
        // Create new user
        User user;
        user.username = username;
        user.role = role;
        user.salt = generateSalt();
        user.passwordHash = hashPassword(password, user.salt);
        user.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        users_[username] = user;
        saveUsers();
        
        QJsonObject success{{"kind", "auth"}, {"event", "ok"}, {"message", "Registration successful"}};
        c->sock->write(buildPacket(MSG_SERVER_EVENT, success));
        
    } else if (op == "login") {
        const QString username = p.json.value("username").toString().trimmed();
        const QString password = p.json.value("password").toString();
        
        QString token;
        if (authenticateUser(username, password, "", &token)) {
            const User& user = users_[username];
            c->token = token;
            c->user = username;
            c->role = user.role;
            
            QJsonObject profile;
            profile["username"] = username;
            profile["role"] = user.role;
            profile["created_at"] = user.createdAt;
            
            QJsonObject success{{"kind", "auth"}, {"event", "ok"}, {"token", token}, {"user", profile}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, success));
        } else {
            QJsonObject error{{"kind", "auth"}, {"event", "error"}, {"message", "Invalid username or password"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
        }
    } else {
        QJsonObject error{{"kind", "auth"}, {"event", "error"}, {"message", "Unknown operation"}};
        c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
    }
}

void RoomHub::handleOrder(ClientCtx* c, const Packet& p) {
    const QString op = p.json.value("op").toString();
    
    if (op == "create") {
        if (c->role != "factory") {
            QJsonObject error{{"code", 403}, {"message", "Only factory users can create orders"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
            return;
        }
        
        const QString title = p.json.value("title").toString().trimmed();
        const QString description = p.json.value("description").toString();
        
        if (title.isEmpty()) {
            QJsonObject error{{"code", 400}, {"message", "Title is required"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
            return;
        }
        
        QString orderId = createWorkOrder(title, description, c->user);
        WorkOrder& order = workOrders_[orderId];
        
        QJsonObject orderJson;
        orderJson["id"] = order.id;
        orderJson["title"] = order.title;
        orderJson["description"] = order.description;
        orderJson["status"] = order.status;
        orderJson["created_by"] = order.createdBy;
        orderJson["assigned_to"] = order.assignedTo;
        orderJson["created_at"] = order.createdAt;
        orderJson["updated_at"] = order.updatedAt;
        
        QJsonObject response{{"op", "create"}, {"order", orderJson}};
        c->sock->write(buildPacket(MSG_ORDER, response));
        
        broadcastWorkOrderEvent("created", order);
        
    } else if (op == "list") {
        QJsonArray orders = getWorkOrdersForUser(c->user, c->role);
        QJsonObject response{{"op", "list"}, {"orders", orders}};
        c->sock->write(buildPacket(MSG_ORDER, response));
        
    } else if (op == "accept") {
        if (c->role != "expert") {
            QJsonObject error{{"code", 403}, {"message", "Only expert users can accept orders"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
            return;
        }
        
        const QString orderId = p.json.value("id").toString();
        if (acceptWorkOrder(orderId, c->user)) {
            WorkOrder& order = workOrders_[orderId];
            
            QJsonObject orderJson;
            orderJson["id"] = order.id;
            orderJson["title"] = order.title;
            orderJson["status"] = order.status;
            orderJson["assigned_to"] = order.assignedTo;
            orderJson["updated_at"] = order.updatedAt;
            
            QJsonObject response{{"op", "accept"}, {"order", orderJson}};
            c->sock->write(buildPacket(MSG_ORDER, response));
            
            broadcastWorkOrderEvent("accepted", order);
        } else {
            QJsonObject error{{"code", 400}, {"message", "Cannot accept order"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
        }
        
    } else if (op == "status") {
        const QString orderId = p.json.value("id").toString();
        const QString newStatus = p.json.value("status").toString();
        
        if (updateWorkOrderStatus(orderId, newStatus, c->user)) {
            WorkOrder& order = workOrders_[orderId];
            
            QJsonObject orderJson;
            orderJson["id"] = order.id;
            orderJson["status"] = order.status;
            orderJson["updated_at"] = order.updatedAt;
            
            QJsonObject response{{"op", "status"}, {"order", orderJson}};
            c->sock->write(buildPacket(MSG_ORDER, response));
            
            broadcastWorkOrderEvent("updated", order);
        } else {
            QJsonObject error{{"code", 400}, {"message", "Cannot update order status"}};
            c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
        }
        
    } else {
        QJsonObject error{{"code", 400}, {"message", "Unknown operation"}};
        c->sock->write(buildPacket(MSG_SERVER_EVENT, error));
    }
}

void RoomHub::handlePing(ClientCtx* c, const Packet& p) {
    QJsonObject pong = p.json;
    pong["ts"] = QDateTime::currentMSecsSinceEpoch();
    c->sock->write(buildPacket(MSG_PONG, pong));
}

// Room management (keep existing implementation)
void RoomHub::joinRoom(ClientCtx* c, const QString& roomId) {
    const QString oldRoom = c->roomId;
    
    // Leave old room if any
    if (!oldRoom.isEmpty()) {
        auto range = rooms_.equal_range(oldRoom);
        for (auto i = range.first; i != range.second; ) {
            if (i.value() == c->sock) i = rooms_.erase(i);
            else ++i;
        }
        broadcastRoomMembers(oldRoom, "leave", c->user);
    }
    
    // Join new room
    c->roomId = roomId;
    rooms_.insert(roomId, c->sock);
    
    qInfo() << "User" << c->user << "joined room" << roomId;
}

void RoomHub::broadcastToRoom(const QString& roomId, const QByteArray& packet, QTcpSocket* except, bool dropVideoIfBacklog) {
    auto range = rooms_.equal_range(roomId);
    for (auto it = range.first; it != range.second; ++it) {
        QTcpSocket* sock = it.value();
        if (sock == except || sock->state() != QAbstractSocket::ConnectedState) continue;
        
        // Implement congestion control for video frames
        if (dropVideoIfBacklog && sock->bytesToWrite() > kBacklogDropThreshold) {
            // Notify sender about congestion
            if (except) {
                QJsonObject congestion{{"kind", "net"}, {"event", "congested"}, {"backlog_bytes", sock->bytesToWrite()}};
                except->write(buildPacket(MSG_SERVER_EVENT, congestion));
            }
            continue; // Drop this frame
        }
        
        sock->write(packet);
    }
}

QStringList RoomHub::listMembers(const QString& roomId) const {
    QStringList members;
    auto range = rooms_.equal_range(roomId);
    for (auto it = range.first; it != range.second; ++it) {
        auto clientIt = clients_.find(it.value());
        if (clientIt != clients_.end()) {
            members << clientIt.value()->user;
        }
    }
    return members;
}

void RoomHub::broadcastRoomMembers(const QString& roomId, const QString& event, const QString& whoChanged) {
    QStringList members = listMembers(roomId);
    QJsonObject payload{{"kind", "room"}, {"event", event}, {"roomId", roomId}, {"members", QJsonArray::fromStringList(members)}, {"user", whoChanged}};
    QByteArray packet = buildPacket(MSG_SERVER_EVENT, payload);
    broadcastToRoom(roomId, packet);
}

void RoomHub::sendRoomMembersTo(QTcpSocket* target, const QString& roomId, const QString& event, const QString& whoChanged) {
    QStringList members = listMembers(roomId);
    QJsonObject payload{{"kind", "room"}, {"event", event}, {"roomId", roomId}, {"members", QJsonArray::fromStringList(members)}, {"user", whoChanged}};
    target->write(buildPacket(MSG_SERVER_EVENT, payload));
}

// Authentication and persistence implementation
bool RoomHub::loadUsers() {
    QFile file(QDir(dataPath_).absoluteFilePath("users.json"));
    if (!file.exists()) {
        qInfo() << "Users file does not exist, starting with empty user database";
        return true;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot read users file:" << file.errorString();
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Invalid JSON in users file:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QJsonObject userObj = it.value().toObject();
        User user;
        user.username = it.key();
        user.passwordHash = userObj["password_hash"].toString();
        user.salt = userObj["salt"].toString();
        user.role = userObj["role"].toString();
        user.createdAt = userObj["created_at"].toString();
        users_[user.username] = user;
    }
    
    qInfo() << "Loaded" << users_.size() << "users from file";
    return true;
}

bool RoomHub::saveUsers() {
    QDir dir(dataPath_);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "Cannot create data directory";
        return false;
    }
    
    QJsonObject root;
    for (auto it = users_.begin(); it != users_.end(); ++it) {
        const User& user = it.value();
        QJsonObject userObj;
        userObj["password_hash"] = user.passwordHash;
        userObj["salt"] = user.salt;
        userObj["role"] = user.role;
        userObj["created_at"] = user.createdAt;
        root[user.username] = userObj;
    }
    
    QSaveFile file(dir.absoluteFilePath("users.json"));
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write users file:" << file.errorString();
        return false;
    }
    
    file.write(QJsonDocument(root).toJson());
    if (!file.commit()) {
        qWarning() << "Cannot commit users file:" << file.errorString();
        return false;
    }
    
    qInfo() << "Saved" << users_.size() << "users to file";
    return true;
}

bool RoomHub::loadWorkOrders() {
    QFile file(QDir(dataPath_).absoluteFilePath("workorders.json"));
    if (!file.exists()) {
        qInfo() << "Work orders file does not exist, starting with empty orders database";
        return true;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot read work orders file:" << file.errorString();
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Invalid JSON in work orders file:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QJsonObject orderObj = it.value().toObject();
        WorkOrder order;
        order.id = it.key();
        order.title = orderObj["title"].toString();
        order.description = orderObj["description"].toString();
        order.status = orderObj["status"].toString();
        order.createdBy = orderObj["created_by"].toString();
        order.assignedTo = orderObj["assigned_to"].toString();
        order.createdAt = orderObj["created_at"].toString();
        order.updatedAt = orderObj["updated_at"].toString();
        workOrders_[order.id] = order;
    }
    
    qInfo() << "Loaded" << workOrders_.size() << "work orders from file";
    return true;
}

bool RoomHub::saveWorkOrders() {
    QDir dir(dataPath_);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "Cannot create data directory";
        return false;
    }
    
    QJsonObject root;
    for (auto it = workOrders_.begin(); it != workOrders_.end(); ++it) {
        const WorkOrder& order = it.value();
        QJsonObject orderObj;
        orderObj["title"] = order.title;
        orderObj["description"] = order.description;
        orderObj["status"] = order.status;
        orderObj["created_by"] = order.createdBy;
        orderObj["assigned_to"] = order.assignedTo;
        orderObj["created_at"] = order.createdAt;
        orderObj["updated_at"] = order.updatedAt;
        root[order.id] = orderObj;
    }
    
    QSaveFile file(dir.absoluteFilePath("workorders.json"));
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write work orders file:" << file.errorString();
        return false;
    }
    
    file.write(QJsonDocument(root).toJson());
    if (!file.commit()) {
        qWarning() << "Cannot commit work orders file:" << file.errorString();
        return false;
    }
    
    qInfo() << "Saved" << workOrders_.size() << "work orders to file";
    return true;
}

QString RoomHub::generateToken() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString RoomHub::hashPassword(const QString& password, const QString& salt) {
    QByteArray data = (password + salt).toUtf8();
    return QString(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString RoomHub::generateSalt() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool RoomHub::authenticateUser(const QString& username, const QString& password, const QString& role, QString* outToken) {
    auto it = users_.find(username);
    if (it == users_.end()) return false;
    
    const User& user = it.value();
    if (!role.isEmpty() && user.role != role) return false;
    
    QString hashedPassword = hashPassword(password, user.salt);
    if (hashedPassword != user.passwordHash) return false;
    
    // Generate and store session token
    QString token = generateToken();
    sessions_[token] = username;
    
    if (outToken) *outToken = token;
    return true;
}

bool RoomHub::isValidToken(const QString& token, QString* outUsername, QString* outRole) {
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    
    const QString& username = it.value();
    auto userIt = users_.find(username);
    if (userIt == users_.end()) {
        sessions_.remove(token); // Clean up orphaned session
        return false;
    }
    
    if (outUsername) *outUsername = username;
    if (outRole) *outRole = userIt.value().role;
    return true;
}

QString RoomHub::createWorkOrder(const QString& title, const QString& description, const QString& createdBy) {
    QString orderId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    WorkOrder order;
    order.id = orderId;
    order.title = title;
    order.description = description;
    order.status = "open";
    order.createdBy = createdBy;
    order.assignedTo = "";
    order.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    order.updatedAt = order.createdAt;
    
    workOrders_[orderId] = order;
    saveWorkOrders();
    
    return orderId;
}

QJsonArray RoomHub::getWorkOrdersForUser(const QString& username, const QString& role) {
    QJsonArray orders;
    
    for (auto it = workOrders_.begin(); it != workOrders_.end(); ++it) {
        const WorkOrder& order = it.value();
        bool include = false;
        
        if (role == "factory" && order.createdBy == username) {
            include = true;
        } else if (role == "expert" && (order.status == "open" || order.assignedTo == username)) {
            include = true;
        }
        
        if (include) {
            QJsonObject orderObj;
            orderObj["id"] = order.id;
            orderObj["title"] = order.title;
            orderObj["description"] = order.description;
            orderObj["status"] = order.status;
            orderObj["created_by"] = order.createdBy;
            orderObj["assigned_to"] = order.assignedTo;
            orderObj["created_at"] = order.createdAt;
            orderObj["updated_at"] = order.updatedAt;
            orders.append(orderObj);
        }
    }
    
    return orders;
}

bool RoomHub::acceptWorkOrder(const QString& orderId, const QString& expertUsername) {
    auto it = workOrders_.find(orderId);
    if (it == workOrders_.end()) return false;
    
    WorkOrder& order = it.value();
    if (order.status != "open") return false;
    
    order.status = "assigned";
    order.assignedTo = expertUsername;
    order.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    saveWorkOrders();
    return true;
}

bool RoomHub::updateWorkOrderStatus(const QString& orderId, const QString& newStatus, const QString& username) {
    auto it = workOrders_.find(orderId);
    if (it == workOrders_.end()) return false;
    
    WorkOrder& order = it.value();
    
    // Check permissions
    bool canUpdate = false;
    if (order.createdBy == username) {
        // Factory can update status for their orders
        canUpdate = true;
    } else if (order.assignedTo == username) {
        // Expert can update status for assigned orders
        canUpdate = true;
    }
    
    if (!canUpdate) return false;
    
    // Validate status transitions
    if (newStatus == "closed" && order.createdBy != username) return false;
    if (newStatus == "canceled" && order.createdBy != username) return false;
    
    order.status = newStatus;
    order.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    saveWorkOrders();
    return true;
}

WorkOrder* RoomHub::findWorkOrder(const QString& orderId) {
    auto it = workOrders_.find(orderId);
    return (it != workOrders_.end()) ? &it.value() : nullptr;
}

void RoomHub::broadcastWorkOrderEvent(const QString& event, const WorkOrder& order) {
    QJsonObject orderObj;
    orderObj["id"] = order.id;
    orderObj["title"] = order.title;
    orderObj["description"] = order.description;
    orderObj["status"] = order.status;
    orderObj["created_by"] = order.createdBy;
    orderObj["assigned_to"] = order.assignedTo;
    orderObj["created_at"] = order.createdAt;
    orderObj["updated_at"] = order.updatedAt;
    
    QJsonObject payload{{"kind", "order"}, {"event", event}, {"order", orderObj}};
    QByteArray packet = buildPacket(MSG_SERVER_EVENT, payload);
    
    // Broadcast to all authenticated clients
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        ClientCtx* c = it.value();
        if (!c->token.isEmpty() && isValidToken(c->token)) {
            c->sock->write(packet);
        }
    }
}