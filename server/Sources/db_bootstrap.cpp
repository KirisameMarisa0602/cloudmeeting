#include "db_bootstrap.h"

static bool execSql(QSqlDatabase& db, const QString& sql, QString* err = nullptr)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        if (err) *err = q.lastError().text() + " | SQL: " + sql;
        return false;
    }
    return true;
}

static bool columnExists(QSqlDatabase& db, const QString& table, const QString& col)
{
    QSqlQuery q(db);
    q.prepare("PRAGMA table_info(" + table + ")");
    if (!q.exec()) return false;
    while (q.next()) {
        if (q.value(1).toString().compare(col, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

static bool ensureUsersTables(QSqlDatabase& db, QString* err)
{
    // 工厂用户：account 登录，username 展示
    {
        const QString sql =
            "CREATE TABLE IF NOT EXISTS factory_users ("
            "  account  TEXT NOT NULL UNIQUE,"
            "  username TEXT NOT NULL,"
            "  password TEXT NOT NULL"
            ");";
        if (!execSql(db, sql, err)) return false;
        if (!execSql(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_factory_users_account  ON factory_users(account);", err)) return false;
        if (!execSql(db, "CREATE INDEX        IF NOT EXISTS idx_factory_users_username ON factory_users(username);", err)) return false;

        // 旧库兼容：若无 account 列（极小概率），可在此添加并回填；当前为新库路径，略
    }
    // 专家用户
    {
        const QString sql =
            "CREATE TABLE IF NOT EXISTS expert_users ("
            "  account  TEXT NOT NULL UNIQUE,"
            "  username TEXT NOT NULL,"
            "  password TEXT NOT NULL"
            ");";
        if (!execSql(db, sql, err)) return false;
        if (!execSql(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_expert_users_account  ON expert_users(account);", err)) return false;
        if (!execSql(db, "CREATE INDEX        IF NOT EXISTS idx_expert_users_username ON expert_users(username);", err)) return false;
    }
    return true;
}

static bool ensureOrdersTable(QSqlDatabase& db, QString* err)
{
    const QString createSql =
        "CREATE TABLE IF NOT EXISTS orders ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  \"desc\" TEXT,"
        "  status TEXT NOT NULL DEFAULT '待处理',"
        "  factory_account TEXT,"  // 工厂账号（登录名）
        "  expert_account  TEXT,"  // 专家账号（登录名）—接单后写
        "  publisher       TEXT,"  // 发布者用户名（展示名）
        "  accepter        TEXT,"  // 接受者用户名（展示名）
        "  created_at      TEXT NOT NULL DEFAULT (datetime('now'))"
        ");";
    if (!execSql(db, createSql, err)) return false;

    // 补列以兼容已有库
    struct Col { const char* name; const char* type; };
    const Col cols[] = {
        {"factory_account","TEXT"},
        {"expert_account", "TEXT"},
        {"publisher",      "TEXT"},
        {"accepter",       "TEXT"},
    };
    for (const auto& c : cols) {
        if (!columnExists(db, "orders", c.name)) {
            if (!execSql(db, QString("ALTER TABLE orders ADD COLUMN %1 %2;").arg(c.name, c.type), err)) return false;
        }
    }

    // 索引
    if (!execSql(db, "CREATE INDEX IF NOT EXISTS idx_orders_status          ON orders(status);", err)) return false;
    if (!execSql(db, "CREATE INDEX IF NOT EXISTS idx_orders_factory_account ON orders(factory_account);", err)) return false;
    if (!execSql(db, "CREATE INDEX IF NOT EXISTS idx_orders_expert_account  ON orders(expert_account);", err)) return false;
    if (!execSql(db, "CREATE INDEX IF NOT EXISTS idx_orders_publisher       ON orders(publisher);", err)) return false;
    if (!execSql(db, "CREATE INDEX IF NOT EXISTS idx_orders_accepter        ON orders(accepter);", err)) return false;

    return true;
}

bool ensureSchema(QSqlDatabase& db, QString* errorOut)
{
    if (!db.isOpen()) { if (errorOut) *errorOut = "Database is not open"; return false; }
    if (!execSql(db, "PRAGMA foreign_keys = ON;", errorOut)) return false;

    // 版本位
    int userVersion = 0;
    {
        QSqlQuery q("PRAGMA user_version;", db);
        if (q.next()) userVersion = q.value(0).toInt();
    }

    if (userVersion < 1) {
        if (!ensureUsersTables(db, errorOut)) return false;
        if (!ensureOrdersTable(db, errorOut)) return false;

        // 若 publisher 为空但 factory_account 有值，回填
        const QString backfillPub =
            "UPDATE orders SET publisher = ("
            "  SELECT fu.username FROM factory_users fu WHERE fu.account = orders.factory_account"
            ") WHERE (publisher IS NULL OR publisher='') AND factory_account IS NOT NULL;";
        if (!execSql(db, backfillPub, errorOut)) return false;

        if (!execSql(db, "PRAGMA user_version = 1;", errorOut)) return false;
    }
    return true;
}
