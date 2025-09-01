#include "server_actions.h"
#include <QtCore>
#include <QtSql>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
// 工具：执行 prepare+bind+exec
static bool prepareAndBind(QSqlQuery& q, const QString& sql, const QList<QVariant>& binds, QString* err)
{
    if (!q.prepare(sql)) { if (err) *err = q.lastError().text(); return false; }
    for (const auto& v : binds) q.addBindValue(v);
    if (!q.exec()) { if (err) *err = q.lastError().text(); return false; }
    return true;
}

// 注册：role= factory|expert；写入 account / username / password
QJsonObject handleRegister(const QJsonObject& req, QSqlDatabase& db)
{
    QJsonObject resp{{"ok", false}};
    const QString role     = req.value("role").toString();
    const QString account  = req.value("account").toString();
    const QString password = req.value("password").toString();
    const QString username = req.value("username").toString();
    if (role.isEmpty() || account.isEmpty() || password.isEmpty() || username.isEmpty()) { resp["msg"]="参数不完整"; return resp; }

    const QString table = (role == "factory") ? "factory_users" : "expert_users";
    QSqlQuery q(db);
    QString err;

    if (!prepareAndBind(q, QString("INSERT INTO %1(account,username,password) VALUES(?,?,?)").arg(table),
                        {account, username, password}, &err)) { resp["msg"]=err; return resp; }

    resp["ok"] = true;
    return resp;
}

// 登录：使用 account+password，返回 username
QJsonObject handleLogin(const QJsonObject& req, QSqlDatabase& db)
{
    QJsonObject resp{{"ok", false}};
    const QString role     = req.value("role").toString();
    const QString account  = req.value("account").toString();
    const QString password = req.value("password").toString();
    if (role.isEmpty() || account.isEmpty() || password.isEmpty()) { resp["msg"]="参数不完整"; return resp; }

    const QString table = (role == "factory") ? "factory_users" : "expert_users";
    QSqlQuery q(db);
    if (!q.prepare(QString("SELECT username FROM %1 WHERE account=? AND password=?").arg(table))) { resp["msg"]="SQL错误"; return resp; }
    q.addBindValue(account); q.addBindValue(password);
    if (!q.exec()) { resp["msg"]=q.lastError().text(); return resp; }
    if (!q.next()) { resp["msg"]="账号或密码错误"; return resp; }

    resp["ok"] = true;
    resp["username"] = q.value(0).toString();
    return resp;
}

// 新建工单（工厂端）：写入 factory_account 与 publisher(用户名)
QJsonObject handleNewOrder(const QJsonObject& req, QSqlDatabase& db)
{
    QJsonObject resp{{"ok", false}};
    const QString title   = req.value("title").toString();
    const QString desc    = req.value("desc").toString();
    const QString fAcc    = req.value("factory_account").toString();
    const QString pub     = req.value("publisher").toString();
    if (title.isEmpty() || fAcc.isEmpty() || pub.isEmpty()) { resp["msg"]="参数不完整"; return resp; }

    QSqlQuery q(db);
    QString err;
    if (!prepareAndBind(q,
        "INSERT INTO orders(title,\"desc\",status,factory_account,publisher) VALUES(?,?,'待处理',?,?)",
        {title, desc, fAcc, pub}, &err)) { resp["msg"]=err; return resp; }

    resp["ok"] = true;
    return resp;
}

// 更新工单（专家端接受/拒绝）：接受时写 expert_account 与 accepter(用户名)
QJsonObject handleUpdateOrder(const QJsonObject& req, QSqlDatabase& db)
{
    QJsonObject resp{{"ok", false}};
    const int id          = req.value("id").toInt();
    const QString status  = req.value("status").toString();
    const QString eAcc    = req.value("expert_account").toString();
    const QString accepter= req.value("accepter").toString();

    if (id <= 0 || status.isEmpty()) { resp["msg"]="参数不完整"; return resp; }

    QSqlQuery q(db);
    QString err;
    if (status == QStringLiteral("已接受")) {
        if (!prepareAndBind(q,
            "UPDATE orders SET status=?, expert_account=?, accepter=? WHERE id=?",
            {status, eAcc, accepter, id}, &err)) { resp["msg"]=err; return resp; }
    } else {
        if (!prepareAndBind(q,
            "UPDATE orders SET status=? WHERE id=?",
            {status, id}, &err)) { resp["msg"]=err; return resp; }
    }

    resp["ok"] = true;
    return resp;
}

// 查询工单：返回 publisher/accepter（为空时按账号回退显示用户名）
QJsonObject handleGetOrders(const QJsonObject& req, QSqlDatabase& db)
{
    Q_UNUSED(req);
    QJsonObject resp{{"ok", false}};

    QString sql =
        "SELECT o.id, o.title, o.\"desc\", o.status,"
        " COALESCE(o.publisher, fu.username) AS publisher,"
        " COALESCE(o.accepter, eu.username)  AS accepter"
        " FROM orders o"
        " LEFT JOIN factory_users fu ON fu.account = o.factory_account"
        " LEFT JOIN expert_users  eu ON eu.account = o.expert_account"
        " WHERE 1=1";
    QList<QVariant> binds;

    const QString status = req.value("status").toString();
    const QString keyword= req.value("keyword").toString();
    if (!status.isEmpty() && status != QStringLiteral("全部")) { sql += " AND o.status=?"; binds << status; }
    if (!keyword.isEmpty()) {
        sql += " AND (CAST(o.id AS TEXT) LIKE ? OR o.title LIKE ? OR o.\"desc\" LIKE ?)";
        const QString kw = "%" + keyword + "%";
        binds << kw << kw << kw;
    }
    sql += " ORDER BY o.id DESC;";

    QSqlQuery q(db);
    if (!q.prepare(sql)) { resp["msg"]="SQL错误"; return resp; }
    for (int i=0;i<binds.size();++i) q.bindValue(i, binds[i]);
    if (!q.exec()) { resp["msg"]=q.lastError().text(); return resp; }

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
    resp["ok"] = true;
    resp["orders"] = arr;
    return resp;
}
