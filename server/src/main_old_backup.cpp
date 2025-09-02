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

static const quint16 DEFAULT_PORT = 5555;

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
        }
        userOf().remove(s);
        roomOf().remove(s);

        if (notify && !room.isEmpty() && !user.isEmpty()){
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
    static void broadcast(const QString& room, const QJsonObject& obj){
        const QByteArray line = toLine(obj);
        auto range = sockets().equal_range(room);
        for (auto it = range.first; it != range.second; ++it){
            if (QTcpSocket* s = it.value()){ s->write(line); s->flush(); }
        }
    }
};

static bool ensureSchema(QSqlDatabase& db, QString* err=nullptr)
{
    QSqlQuery q(db);

    if (!q.exec("CREATE TABLE IF NOT EXISTS expert_users ("
                " username TEXT PRIMARY KEY,"
                " password TEXT NOT NULL)")) {
        if (err) { *err = q.lastError().text(); }
        return false;
    }

    if (!q.exec("CREATE TABLE IF NOT EXISTS factory_users ("
                " username TEXT PRIMARY KEY,"
                " password TEXT NOT NULL)")) {
        if (err) { *err = q.lastError().text(); }
        return false;
    }

    if (!q.exec("CREATE TABLE IF NOT EXISTS orders ("
                " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                " title TEXT NOT NULL,"
                " \"desc\" TEXT NOT NULL,"
                " status TEXT NOT NULL DEFAULT '待处理',"
                " factory_user TEXT NOT NULL,"
                " accepter TEXT NOT NULL DEFAULT ''"
                ")")) {
        if (err) { *err = q.lastError().text(); }
        return false;
    }

    // 旧 orders 表补齐 accepter 列
    if (!q.exec("PRAGMA table_info(orders)")) {
        if (err) { *err = q.lastError().text(); }
        return false;
    }
    bool hasAccepter = false;
    while (q.next()) if (q.value(1).toString() == "accepter") { hasAccepter = true; break; }
    if (!hasAccepter) {
        QSqlQuery a(db);
        if (!a.exec("ALTER TABLE orders ADD COLUMN accepter TEXT NOT NULL DEFAULT ''")) {
            if (err) { *err = a.lastError().text(); }
            return false;
        }
    }
    return true;
}

static bool userExists(QSqlDatabase& db, const QString& table, const QString& username)
{
    QSqlQuery q(db);
    q.prepare(QString("SELECT 1 FROM %1 WHERE username=? LIMIT 1").arg(table));
    q.addBindValue(username);
    if (!q.exec()) return false;
    return q.next();
}

static QJsonObject handleRegister(const QJsonObject& req, QSqlDatabase& db)
{
    const QString role = req.value("role").toString();
    const QString username = req.value("username").toString().trimmed();
    const QString password = req.value("password").toString();

    if (role != "expert" && role != "factory") return errReply("invalid role");
    if (username.isEmpty() || password.isEmpty()) return errReply("账号或密码为空");

    const QString table = (role == "expert") ? "expert_users" : "factory_users";

    if (userExists(db, table, username)) {
        return errReply("用户已存在");
    }

    QSqlQuery q(db);
    q.prepare(QString("INSERT INTO %1(username,password) VALUES(?,?)").arg(table));
    q.addBindValue(username);
    q.addBindValue(sha256(password));
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());

    return okReply();
}

static QJsonObject handleLogin(const QJsonObject& req, QSqlDatabase& db)
{
    const QString role = req.value("role").toString();
    const QString username = req.value("username").toString().trimmed();
    const QString password = req.value("password").toString();

    if (role != "expert" && role != "factory") return errReply("invalid role");
    if (username.isEmpty() || password.isEmpty()) return errReply("账号或密码为空");

    const QString table = (role == "expert") ? "expert_users" : "factory_users";

    QSqlQuery q(db);
    q.prepare(QString("SELECT 1 FROM %1 WHERE username=? AND password=? LIMIT 1").arg(table));
    q.addBindValue(username);
    q.addBindValue(sha256(password));
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());
    if (!q.next()) return errReply("账号或密码不正确");

    return okReply();
}

static QJsonObject handleNewOrder(const QJsonObject& req, QSqlDatabase& db)
{
    const QString title = req.value("title").toString().trimmed();
    const QString desc  = req.value("desc").toString().trimmed();
    const QString factoryUser = req.value("factory_user").toString().trimmed();

    if (title.isEmpty() || desc.isEmpty() || factoryUser.isEmpty())
        return errReply("参数不完整");

    QSqlQuery q(db);
    q.prepare("INSERT INTO orders(title, \"desc\", status, factory_user, accepter) VALUES(?, ?, '待处理', ?, '')");
    q.addBindValue(title);
    q.addBindValue(desc);
    q.addBindValue(factoryUser);
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());

    return okReply(QJsonObject{{"id", q.lastInsertId().toInt()}});
}

static QJsonObject handleGetOrders(const QJsonObject& req, QSqlDatabase& db)
{
    const QString role = req.value("role").toString();
    const QString username = req.value("username").toString();
    const QString keyword = req.value("keyword").toString();
    const QString status  = req.value("status").toString();

    QString sql = "SELECT id, title, \"desc\", status, factory_user, accepter FROM orders WHERE 1=1";
    QList<QVariant> binds;

    if (role == "factory" && !username.isEmpty()) {
        sql += " AND factory_user=?";
        binds << username;
    }
    if (!keyword.isEmpty()) {
        sql += " AND (title LIKE ? OR \"desc\" LIKE ?)";
        const QString like = "%" + keyword + "%";
        binds << like << like;
    }
    if (!status.isEmpty() && status != QStringLiteral("全部")) {
        sql += " AND status=?";
        binds << status;
    }
    sql += " ORDER BY id DESC";

    QSqlQuery q(db);
    if (!q.prepare(sql)) return errReply("数据库错误: " + q.lastError().text());
    for (const auto& v : binds) q.addBindValue(v);
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        QJsonObject o{
            {"id", q.value(0).toInt()},
            {"title", q.value(1).toString()},
            {"desc", q.value(2).toString()},
            {"status", q.value(3).toString()},
            {"publisher", q.value(4).toString()},
            {"accepter", q.value(5).toString()}
        };
        arr.push_back(o);
    }
    return okReply(QJsonObject{{"orders", arr}});
}

static QJsonObject handleUpdateOrder(const QJsonObject& req, QSqlDatabase& db)
{
    const int id = req.value("id").toInt();
    const QString status = req.value("status").toString().trimmed();
    const QString operatorName = req.value("accepter").toString().trimmed(); // 操作人（专家用户名）

    if (id <= 0 || status.isEmpty()) return errReply("参数不完整");

    // 读取当前状态与接受者
    QSqlQuery qsel(db);
    qsel.prepare("SELECT status, accepter FROM orders WHERE id=?");
    qsel.addBindValue(id);
    if (!qsel.exec()) return errReply("数据库错误: " + qsel.lastError().text());
    if (!qsel.next()) return errReply("工单不存在");

    const QString curStatus = qsel.value(0).toString();
    const QString curAccepter = qsel.value(1).toString();

    // 权限与一致性校验
    if (status == QStringLiteral("已接受")) {
        if (operatorName.isEmpty())
            return errReply("接受工单需要提供 accepter（专家用户名）");
        if (!curAccepter.isEmpty() && curAccepter != operatorName) {
            return errReply("该工单已被其他专家接受，无法再次接受");
        }
        QSqlQuery qup(db);
        qup.prepare("UPDATE orders SET status=?, accepter=? WHERE id=?");
        qup.addBindValue(status);
        qup.addBindValue(operatorName);
        qup.addBindValue(id);
        if (!qup.exec()) return errReply("数据库错误: " + qup.lastError().text());
        return okReply();
    } else {
        // 回滚或拒绝：仅允许“当前无接受者”或“由当前专家接受”的工单
        if (!curAccepter.isEmpty() && curAccepter != operatorName) {
            return errReply("无权限修改：仅原接受者可修改该工单状态");
        }
        QSqlQuery qup(db);
        qup.prepare("UPDATE orders SET status=?, accepter='' WHERE id=?");
        qup.addBindValue(status);
        qup.addBindValue(id);
        if (!qup.exec()) return errReply("数据库错误: " + qup.lastError().text());
        return okReply();
    }
}

static QJsonObject handleDeleteOrder(const QJsonObject& req, QSqlDatabase& db)
{
    const int id = req.value("id").toInt();
    const QString username = req.value("username").toString();

    if (id <= 0 || username.isEmpty()) return errReply("参数不完整");

    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM orders WHERE id=? AND factory_user=?");
    q.addBindValue(id);
    q.addBindValue(username);
    if (!q.exec() || !q.next()) return errReply("只能销毁自己创建的工单");

    q.prepare("DELETE FROM orders WHERE id=?");
    q.addBindValue(id);
    if (!q.exec()) return errReply("数据库错误: " + q.lastError().text());
    return okReply();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath());
    if (!db.open()) { qCritical() << "Open DB failed:" << db.lastError().text(); return 1; }
    { QString err; if (!ensureSchema(db, &err)) { qCritical() << "Ensure schema failed:" << err; return 2; } }

    quint16 port = DEFAULT_PORT;
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

                        else if (action == "register")       resp = handleRegister(req, db);
                        else if (action == "login")          resp = handleLogin(req, db);
                        else if (action == "new_order")      resp = handleNewOrder(req, db);
                        else if (action == "get_orders")     resp = handleGetOrders(req, db);
                        else if (action == "update_order")   resp = handleUpdateOrder(req, db);
                        else if (action == "delete_order")   resp = handleDeleteOrder(req, db);
                        else                                 resp = errReply("unknown action");
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
    qInfo() << "Server listening on" << port << "DB:" << dbPath();

    return app.exec();
}
