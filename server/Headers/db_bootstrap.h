#pragma once
#include <QtCore>
#include <QtSql>

// 在服务端启动后、开始对外提供服务前调用：
// - 不存在 DB 文件 => 自动创建所有表与索引
// - 存在但缺列   => 自动补齐缺失列（publisher/accepter/expert_account 等）
// - 用户表：按“账号(account)+用户名(username)+密码(password)”存储；分表 factory_users / expert_users
// - 订单表：包含 factory_account / expert_account 与 publisher / accepter（用户名）
bool ensureSchema(QSqlDatabase& db, QString* errorOut = nullptr);
