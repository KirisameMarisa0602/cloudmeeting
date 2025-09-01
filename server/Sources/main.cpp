#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QHash>
#include <QMultiHash>

// ————————————————————————————————————————————————————————————————
// 简易行分隔 JSON 协议服务器，集成：注册/登录/工单管理/聊天室广播
// 端口：默认 5555（可通过命令行参数指定）
// ————————————————————————————————————————————————————————————————

// ========== 工具 ==========
static QByteArray toLine(const QJsonObject& o) {
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}
static void writeLine(QTcpSocket* s, const QJsonObject& o) {
    if (!s) return;
    s->write(toLine(o));
    s->flush();
}
static QString dbPath() {
    QDir d(QCoreApplication::applicationDirPath());
    d.mkpath(".");
    return d.filePath("cloudmeeting.db");
}
static QJsonObject okReply(const QJsonObject& extra = {}) {
    QJsonObject r{{"ok", true}};
    for (auto it = extra.begin(); it != extra.end(); ++it) r[it.key()] = it.value();
    return r;
}
static QJsonObject errReply(const QString& msg) {
    return QJsonObject{{"ok", false}, {"msg", msg}};
}

// ========== 数据库自举 ==========
static bool ensureSchema(QSqlDatabase& db, QString* errOut) {
    QSqlQuery q(db);
    // 用户表：role + account 做唯一；username 为展示名
    if (!q.exec("CREATE TABLE IF NOT EXISTS users ("
                " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                " role TEXT NOT NULL,"           // expert | factory
                " account TEXT NOT NULL,"        // 登录账号
                " username TEXT NOT NULL,"       // 展示名
                " password TEXT NOT NULL,"
                " UNIQUE(role, account))")) {
        if (errOut) *errOut = q.lastError().text();
        return false;
    }
    // 工单表：最小化字段，满足客户端读取
    if (!q.exec("CREATE TABLE IF NOT EXISTS orders ("
                " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                " title TEXT NOT NULL,"
                " desc TEXT NOT NULL,"
                " status TEXT NOT NULL DEFAULT '已发布',"  // 已发布/已接受/已拒绝/...
                " publisher TEXT NOT NULL,"               // 工厂端用户名
                " accepter TEXT NOT NULL DEFAULT '' )")) {
        if (errOut) *errOut = q.lastError().text();
        return false;
    }
    return true;
}

// ========== 处理函数（与客户端协议匹配） ==========
static QJsonObject handleRegister(const QJsonObject& req, QSqlDatabase& db) {
    const QString role = req.value("role").toString().trimmed();
    QString account = req.value("account").toString().trimmed();
    const QString username = req.value("username").toString().trimmed();
    const QString password = req.value("password").toString();

    if (role.isEmpty() || username.isEmpty() || password.isEmpty()) {
        return errReply("参数不完整");
    }
    if (account.isEmpty()) account = username; // 兼容“只给 username”的旧客户端

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(1) FROM users WHERE role=? AND account=?");
    q.addBindValue(role);
    q.addBindValue(account);
    if (!q.exec() || !q.next()) return errReply("数据库错误：" + q.lastError().text());
    if (q.value(0).toInt() > 0) return errReply("账号已存在");

    q.prepare("INSERT INTO users(role, account, username, password) VALUES(?,?,?,?)");
    q.addBindValue(role);
    q.addBindValue(account);
    q.addBindValue(username);
    q.addBindValue(password);
    if (!q.exec()) return errReply("数据库错误：" + q.lastError().text());

    return okReply();
}

static QJsonObject handleLogin(const QJsonObject& req, QSqlDatabase& db) {
    const QString role = req.value("role").toString().trimmed();
    QString account = req.value("account").toString().trimmed();
    if (account.isEmpty()) account = req.value("username").toString().trimmed(); // 兼容旧键名
    const QString password = req.value("password").toString();

    if (role.isEmpty() || account.isEmpty() || password.isEmpty()) {
        return errReply("参数不完整");
    }

    QSqlQuery q(db);
    q.prepare("SELECT username FROM users WHERE role=? AND account=? AND password=?");
    q.addBindValue(role);
    q.addBindValue(account);
    q.addBindValue(password);
    if (!q.exec()) return errReply("数据库错误：" + q.lastError().text());
    if (!q.next()) return errReply("账号或密码错误");

    const QString username = q.value(0).toString();
    return okReply(QJsonObject{{"username", username}});
}

static QJsonObject handleNewOrder(const QJsonObject& req, QSqlDatabase& db) {
    const QString title = req.value("title").toString().trimmed();
    const QString desc  = req.value("desc").toString().trimmed();
    QString publisher   = req.value("publisher").toString().trimmed(); // 工厂端用户名
    if (title.isEmpty() || desc.isEmpty()) return errReply("参数不完整");
    if (publisher.isEmpty()) publisher = "未知工厂";

    QSqlQuery q(db);
    q.prepare("INSERT INTO orders(title, desc, status, publisher, accepter) VALUES(?,?,?,?,?)");
    q.addBindValue(title);
    q.addBindValue(desc);
    q.addBindValue(QStringLiteral("已发布"));
    q.addBindValue(publisher);
    q.addBindValue(QString());
    if (!q.exec()) return errReply("数据库错误：" + q.lastError().text());

    const int id = q.lastInsertId().toInt();
    return okReply(QJsonObject{{"id", id}});
}

static QJsonObject handleUpdateOrder(const QJsonObject& req, QSqlDatabase& db) {
    const int id = req.value("id").toInt();
    const QString status = req.value("status").toString().trimmed();
    const QString accepter = req.value("accepter").toString().trimmed(); // 专家用户名

    if (id <= 0 || status.isEmpty()) return errReply("参数不完整");

    QSqlQuery q(db);
    if (accepter.isEmpty()) {
        q.prepare("UPDATE orders SET status=? WHERE id=?");
        q.addBindValue(status);
        q.addBindValue(id);
    } else {
        q.prepare("UPDATE orders SET status=?, accepter=? WHERE id=?");
        q.addBindValue(status);
        q.addBindValue(accepter);
        q.addBindValue(id);
    }
    if (!q.exec()) return errReply("数据库错误：" + q.lastError().text());
    return okReply();
}

static QJsonObject handleGetOrders(const QJsonObject& /*req*/, QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec("SELECT id, title, desc, status, publisher, accepter FROM orders ORDER BY id DESC")) {
        return errReply("数据库错误：" + q.lastError().text());
    }
    QJsonArray arr;
    while (q.next()) {
        QJsonObject it{
            {"id", q.value(0).toInt()},
            {"title", q.value(1).toString()},
            {"desc", q.value(2).toString()},
            {"status", q.value(3).toString()},
            {"publisher", q.value(4).toString()},
            {"accepter", q.value(5).toString()}
        };
        arr.push_back(it);
    }
    return okReply(QJsonObject{{"orders", arr}});
}

static QJsonObject handleDeleteOrder(const QJsonObject& req, QSqlDatabase& db) {
    const int id = req.value("id").toInt();
    if (id <= 0) return errReply("参数不完整");
    QSqlQuery q(db);
    q.prepare("DELETE FROM orders WHERE id=?");
    q.addBindValue(id);
    if (!q.exec()) return errReply("数据库错误：" + q.lastError().text());
    return okReply();
}

// ========== 简易聊天室 ==========
struct ChatHub {
    // socket -> username/room
    static QHash<QTcpSocket*, QString>& userOf() { static QHash<QTcpSocket*, QString> x; return x; }
    static QHash<QTcpSocket*, QString>& roomOf() { static QHash<QTcpSocket*, QString> x; return x; }
    // room -> sockets
    static QMultiHash<QString, QTcpSocket*>& roomSockets() { static QMultiHash<QString, QTcpSocket*> x; return x; }

    static void join(QTcpSocket* s, const QString& room, const QString& user) {
        leave(s, false);
        userOf().insert(s, user);
        roomOf().insert(s, room);
        roomSockets().insert(room, s);

        // 系统广播：用户进入房间
        QJsonObject msg{
            {"action","chat_broadcast"},
            {"system", true},
            {"room", room},
            {"from", user},
            {"text", QString("用户（%1）进入房间").arg(user)}
        };
        broadcast(room, msg);
    }

    static void leave(QTcpSocket* s, bool broadcastLeave = true) {
        const QString user = userOf().value(s);
        const QString room = roomOf().value(s);
        if (!room.isEmpty()) {
            auto range = roomSockets().equal_range(room);
            for (auto it = range.first; it != range.second; ) {
                if (it.value() == s) it = roomSockets().erase(it);
                else ++it;
            }
        }
        userOf().remove(s);
        roomOf().remove(s);

        if (broadcastLeave && !room.isEmpty() && !user.isEmpty()) {
            QJsonObject msg{
                {"action","chat_broadcast"},
                {"system", true},
                {"room", room},
                {"from", user},
                {"text", QString("用户（%1）离开房间").arg(user)}
            };
            broadcast(room, msg);
        }
    }

    static void broadcast(const QString& room, const QJsonObject& obj) {
        QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
        auto range = roomSockets().equal_range(room);
        for (auto it = range.first; it != range.second; ++it) {
            if (QTcpSocket* s = it.value()) {
                s->write(line);
                s->flush();
            }
        }
    }
};

// ========== 服务器主程序 ==========
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // 1) 打开数据库并自举
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath());
    if (!db.open()) {
        qCritical() << "Open DB failed:" << db.lastError().text();
        return 1;
    }
    {
        QString err;
        if (!ensureSchema(db, &err)) {
            qCritical() << "Ensure schema failed:" << err;
            return 2;
        }
    }

    // 2) 启动 TCP 服务器
    quint16 port = 5555;
    if (app.arguments().size() >= 2) {
        bool ok=false; int p = app.arguments().at(1).toInt(&ok);
        if (ok && p>0 && p<65536) port = static_cast<quint16>(p);
    }

    QTcpServer server;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]{
        while (QTcpSocket* sock = server.nextPendingConnection()) {
            sock->setTextModeEnabled(true);

            QObject::connect(sock, &QTcpSocket::readyRead, &server, [sock, &db]{
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

                        // 聊天相关
                        if (action == "chat_join") {
                            const QString room = req.value("room").toString().trimmed();
                            const QString user = req.value("username").toString().trimmed();
                            if (room.isEmpty() || user.isEmpty()) {
                                resp = errReply("参数不完整");
                            } else {
                                ChatHub::join(sock, room, user);
                                resp = okReply(QJsonObject{{"action","chat_join"}});
                            }
                        } else if (action == "chat_msg") {
                            const QString room = req.value("room").toString().trimmed();
                            const QString from = req.value("from").toString().trimmed();
                            const QString text = req.value("text").toString();
                            if (room.isEmpty() || from.isEmpty() || text.isEmpty()) {
                                resp = errReply("参数不完整");
                            } else {
                                QJsonObject msg{
                                    {"action","chat_broadcast"},
                                    {"system", false},
                                    {"room", room},
                                    {"from", from},
                                    {"text", text}
                                };
                                ChatHub::broadcast(room, msg);
                                resp = okReply(QJsonObject{{"action","chat_msg"}});
                            }
                        } else if (action == "chat_leave") {
                            ChatHub::leave(sock, true);
                            resp = okReply(QJsonObject{{"action","chat_leave"}});
                        }

                        // 账号 / 工单
                        else if (action == "register")      resp = handleRegister(req, db);
                        else if (action == "login")         resp = handleLogin(req, db);
                        else if (action == "new_order")     resp = handleNewOrder(req, db);
                        else if (action == "update_order")  resp = handleUpdateOrder(req, db);
                        else if (action == "get_orders")    resp = handleGetOrders(req, db);
                        else if (action == "delete_order")  resp = handleDeleteOrder(req, db);
                        else                                resp = errReply("unknown action");
                    }

                    writeLine(sock, resp);
                }
            });

            QObject::connect(sock, &QTcpSocket::disconnected, &server, [sock]{
                ChatHub::leave(sock, true);
                sock->deleteLater();
            });
        }
    });

    if (!server.listen(QHostAddress::Any, port)) {
        qCritical() << "Listen failed on port" << port << ":" << server.errorString();
        return 3;
    }
    qInfo() << "Server is listening on port" << port << "DB:" << dbPath();

    return app.exec();
}
