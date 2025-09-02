#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QtSql>
#include <QDir>
#include <QCryptographicHash>
#include <QHash>
#include <QMultiHash>

// Include test2 functionality
#include "roomhub.h"
#include "udprelay.h"
#include "protocol.h"

static const quint16 AUTH_ORDER_PORT = 5555;  // Auth/Order server
static const quint16 ROOMHUB_PORT = 9000;     // RoomHub server  
static const quint16 UDPRELAY_PORT = 9001;    // UDP relay

static QByteArray toLine(const QJsonObject& o){ return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n'; }
static void writeLine(QTcpSocket* s, const QJsonObject& o){ if (!s) return; s->write(toLine(o)); s->flush(); }
static QJsonObject okReply(const QJsonObject& extra = {}){ QJsonObject r{{"ok",true}}; for(auto it=extra.begin(); it!=extra.end(); ++it) r[it.key()] = it.value(); return r; }
static QJsonObject errReply(const QString& msg){ return QJsonObject{{"ok",false},{"msg",msg}}; }
static QString dbPath(){ return QDir(QCoreApplication::applicationDirPath()).filePath("cloudmeeting.db"); }
static QString sha256(const QString& s){ return QString(QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256).toHex()); }

// 简单聊天室（保留原有功能）
struct ChatHub {
    static QHash<QTcpSocket*, QString>& userOf(){ static QHash<QTcpSocket*, QString> x; return x; }
    static QHash<QTcpSocket*, QString>& roomOf(){ static QHash<QTcpSocket*, QString> x; return x; }
    static QMultiHash<QString, QTcpSocket*>& sockets(){ static QMultiHash<QString, QTcpSocket*> x; return x; }

    static void join(QTcpSocket* s, const QString& room, const QString& user){
        leave(s, false);
        userOf().insert(s, user);
        roomOf().insert(s, room);
        sockets().insert(room, s);

        QJsonObject msg{
            {"action","chat_broadcast"},
            {"system",true},
            {"room",room},
            {"from",user},
            {"text",QStringLiteral("用户（%1）进入房间").arg(user)}
        };
        broadcast(room, msg);
    }
    static void leave(QTcpSocket* s, bool notify=true){
        const QString user = userOf().value(s);
        const QString room = roomOf().value(s);
        if (!room.isEmpty()) {
            auto range = sockets().equal_range(room);
            for (auto it = range.first; it != range.second; ) {
                if (it.value() == s) it = sockets().erase(it);
                else ++it;
            }
            if (notify) {
                QJsonObject msg{
                    {"action","chat_broadcast"},
                    {"system",true},
                    {"room",room},
                    {"from",user},
                    {"text",QStringLiteral("用户（%1）离开房间").arg(user)}
                };
                broadcast(room, msg);
            }
        }
        userOf().remove(s);
        roomOf().remove(s);
    }
    static void broadcast(const QString& room, const QJsonObject& msg){
        auto range = sockets().equal_range(room);
        for (auto it = range.first; it != range.second; ++it) {
            writeLine(it.value(), msg);
        }
    }
};

static bool ensureSchema(QSqlDatabase& db, QString* err=nullptr)
{
    if (!db.isOpen()) { if (err) *err = "Database not open"; return false; }

    QStringList queries = {
        "CREATE TABLE IF NOT EXISTS factory_users (id INTEGER PRIMARY KEY, account TEXT UNIQUE, username TEXT, password TEXT)",
        "CREATE TABLE IF NOT EXISTS expert_users (id INTEGER PRIMARY KEY, account TEXT UNIQUE, username TEXT, password TEXT)",
        "CREATE TABLE IF NOT EXISTS orders (id INTEGER PRIMARY KEY, title TEXT, \"desc\" TEXT, status TEXT, factory_user TEXT, accepter TEXT, publisher TEXT)"
    };

    for (const QString& sql : queries) {
        QSqlQuery q(db);
        if (!q.exec(sql)) {
            if (err) *err = QStringLiteral("SQL执行失败: %1 (%2)").arg(sql, q.lastError().text());
            return false;
        }
    }
    return true;
}

static bool userExists(QSqlDatabase& db, const QString& table, const QString& username)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM %1 WHERE account=?").arg(table));
    q.addBindValue(username);
    return q.exec() && q.next();
}

static QJsonObject handleRegister(const QJsonObject& req, QSqlDatabase& db)
{
    const QString role     = req.value("role").toString();
    const QString account  = req.value("account").toString();
    const QString password = req.value("password").toString();
    const QString username = req.value("username").toString();
    if (role.isEmpty() || account.isEmpty() || password.isEmpty() || username.isEmpty()) return errReply("参数不完整");

    const QString table = (role == "factory") ? "factory_users" : "expert_users";
    if (userExists(db, table, account)) return errReply("账号已存在");

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO %1(account,username,password) VALUES(?,?,?)").arg(table));
    q.addBindValue(account);
    q.addBindValue(username);
    q.addBindValue(sha256(password));
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());

    return okReply();
}

static QJsonObject handleLogin(const QJsonObject& req, QSqlDatabase& db)
{
    const QString role     = req.value("role").toString();
    const QString account  = req.value("account").toString();
    const QString password = req.value("password").toString();
    if (role.isEmpty() || account.isEmpty() || password.isEmpty()) return errReply("参数不完整");

    const QString table = (role == "factory") ? "factory_users" : "expert_users";
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT username FROM %1 WHERE account=? AND password=?").arg(table));
    q.addBindValue(account);
    q.addBindValue(sha256(password));
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());
    if (!q.next()) return errReply("账号或密码错误");

    return okReply(QJsonObject{{"username", q.value(0).toString()}});
}

static QJsonObject handleNewOrder(const QJsonObject& req, QSqlDatabase& db)
{
    const QString title = req.value("title").toString().trimmed();
    const QString desc  = req.value("desc").toString().trimmed();
    const QString factoryUser = req.value("factory_user").toString().trimmed();

    if (title.isEmpty() || desc.isEmpty() || factoryUser.isEmpty())
        return errReply("参数不完整");

    QSqlQuery q(db);
    q.prepare("INSERT INTO orders(title, \"desc\", status, factory_user, accepter, publisher) "
              "VALUES(?, ?, '待处理', ?, '', ?)");
    q.addBindValue(title);
    q.addBindValue(desc);
    q.addBindValue(factoryUser);
    q.addBindValue(factoryUser); // publisher = factory_user
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());

    const int id = q.lastInsertId().toInt();
    return okReply(QJsonObject{{"id", id}});
}

static QJsonObject handleGetOrders(const QJsonObject& req, QSqlDatabase& db)
{
    QSqlQuery q(db);
    QString sql = "SELECT id, title, \"desc\", status, factory_user, accepter, publisher FROM orders WHERE 1=1";
    
    const QString status = req.value("status").toString().trimmed();
    const QString keyword = req.value("keyword").toString().trimmed();
    const QString role = req.value("role").toString().trimmed();
    const QString username = req.value("username").toString().trimmed();
    
    if (!status.isEmpty() && status != "全部") {
        sql += " AND status = :status";
    }
    if (!keyword.isEmpty()) {
        sql += " AND (title LIKE :keyword OR \"desc\" LIKE :keyword)";
    }
    if (role == "factory" && !username.isEmpty()) {
        sql += " AND factory_user = :factory_user";
    }
    
    sql += " ORDER BY id DESC";
    
    if (!q.prepare(sql)) return errReply("SQL错误");
    
    if (!status.isEmpty() && status != "全部") {
        q.bindValue(":status", status);
    }
    if (!keyword.isEmpty()) {
        q.bindValue(":keyword", QString("%%1%").arg(keyword));
    }
    if (role == "factory" && !username.isEmpty()) {
        q.bindValue(":factory_user", username);
    }
    
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        QJsonObject o;
        o["id"]        = q.value("id").toInt();
        o["title"]     = q.value("title").toString();
        o["desc"]      = q.value("\"desc\"").toString();
        o["status"]    = q.value("status").toString();
        o["publisher"] = q.value("publisher").toString();
        o["accepter"]  = q.value("accepter").toString();
        arr.append(o);
    }
    return okReply(QJsonObject{{"orders", arr}});
}

static QJsonObject handleUpdateOrder(const QJsonObject& req, QSqlDatabase& db)
{
    const int id = req.value("id").toInt();
    const QString status = req.value("status").toString().trimmed();
    if (id <= 0 || status.isEmpty()) return errReply("参数不完整");

    QSqlQuery q(db);
    if (status == "已接受") {
        const QString accepter = req.value("accepter").toString().trimmed();
        if (accepter.isEmpty()) return errReply("接受者参数缺失");
        q.prepare("UPDATE orders SET status=?, accepter=? WHERE id=?");
        q.addBindValue(status);
        q.addBindValue(accepter);
        q.addBindValue(id);
    } else {
        q.prepare("UPDATE orders SET status=? WHERE id=?");
        q.addBindValue(status);
        q.addBindValue(id);
    }
    
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());
    return okReply();
}

static QJsonObject handleDeleteOrder(const QJsonObject& req, QSqlDatabase& db)
{
    const int id = req.value("id").toInt();
    const QString username = req.value("factory_user").toString();
    if (id <= 0 || username.isEmpty()) return errReply("参数不完整");

    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM orders WHERE id=? AND factory_user=?");
    q.addBindValue(id);
    q.addBindValue(username);
    if (!q.exec() || !q.next()) {
        return errReply("只能销毁自己创建的工单");
    }
    q.prepare("DELETE FROM orders WHERE id=?");
    q.addBindValue(id);
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());
    return okReply();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Initialize database
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath());
    if (!db.open()) { 
        qCritical() << "Open DB failed:" << db.lastError().text(); 
        return 1; 
    }
    QString err;
    if (!ensureSchema(db, &err)) { 
        qCritical() << "Ensure schema failed:" << err; 
        return 2; 
    }

    // Start auth/order server on TCP 5555
    QTcpServer authOrderServer;
    QObject::connect(&authOrderServer, &QTcpServer::newConnection, &authOrderServer, [&]{
        while (QTcpSocket* sock = authOrderServer.nextPendingConnection()) {
            sock->setTextModeEnabled(true);

            QObject::connect(sock, &QTcpSocket::readyRead, &authOrderServer, [sock, &db]{
                while (sock->canReadLine()) {
                    const QByteArray line = sock->readLine().trimmed();
                    if (line.isEmpty()) continue;

                    QJsonParseError pe{};
                    const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
                    QJsonObject resp;

                    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                        resp = errReply("invalid json");
                    } else {
                        const QJsonObject req = doc.object();
                        const QString action = req.value("action").toString();

                        // Chat functionality (preserved)
                        if (action == "chat_join") {
                            const QString room = req.value("room").toString().trimmed();
                            const QString user = req.value("username").toString().trimmed();
                            if (room.isEmpty() || user.isEmpty()) resp = errReply("参数不完整");
                            else { ChatHub::join(sock, room, user); resp = okReply(QJsonObject{{"action","chat_join"}}); }
                        } else if (action == "chat_msg") {
                            const QString room = req.value("room").toString().trimmed();
                            const QString from = req.value("from").toString().trimmed();
                            const QString text = req.value("text").toString();
                            if (room.isEmpty() || from.isEmpty() || text.isEmpty()) resp = errReply("参数不完整");
                            else { ChatHub::broadcast(room, QJsonObject{{"action","chat_broadcast"},{"system",false},{"room",room},{"from",from},{"text",text}}); resp = okReply(QJsonObject{{"action","chat_msg"}}); }
                        } else if (action == "chat_leave") {
                            ChatHub::leave(sock, true); resp = okReply(QJsonObject{{"action","chat_leave"}});
                        }

                        // Auth/Order functionality  
                        else if (action == "register")       resp = handleRegister(req, db);
                        else if (action == "login")          resp = handleLogin(req, db);
                        else if (action == "new_order")      resp = handleNewOrder(req, db);
                        else if (action == "get_orders")     resp = handleGetOrders(req, db);
                        else if (action == "update_order")   resp = handleUpdateOrder(req, db);
                        else if (action == "delete_order")   resp = handleDeleteOrder(req, db);

                        else resp = errReply(QStringLiteral("未知动作: %1").arg(action));
                    }

                    writeLine(sock, resp);
                }
            });

            QObject::connect(sock, &QTcpSocket::disconnected, &authOrderServer, [sock]{
                ChatHub::leave(sock, true);
                sock->deleteLater();
            });
        }
    });

    if (!authOrderServer.listen(QHostAddress::Any, AUTH_ORDER_PORT)) {
        qCritical() << "Auth/Order server failed to listen on port" << AUTH_ORDER_PORT << ":" << authOrderServer.errorString();
        return 3;
    }
    qInfo() << "Auth/Order server listening on port" << AUTH_ORDER_PORT;

    // Start RoomHub server on TCP 9000
    RoomHub roomHub;
    if (!roomHub.start(ROOMHUB_PORT)) {
        qCritical() << "RoomHub failed to start on port" << ROOMHUB_PORT;
        return 4;
    }
    qInfo() << "RoomHub server listening on port" << ROOMHUB_PORT;

    // Start UDP relay on UDP 9001
    UdpRelay udpRelay;
    if (!udpRelay.start(UDPRELAY_PORT)) {
        qCritical() << "UDP relay failed to start on port" << UDPRELAY_PORT;
        return 5;
    }
    qInfo() << "UDP relay listening on port" << UDPRELAY_PORT;

    qInfo() << "CloudMeeting server started successfully:";
    qInfo() << "  - Auth/Order: TCP" << AUTH_ORDER_PORT;
    qInfo() << "  - RoomHub:    TCP" << ROOMHUB_PORT;
    qInfo() << "  - UDP Relay:  UDP" << UDPRELAY_PORT;

    return app.exec();
}