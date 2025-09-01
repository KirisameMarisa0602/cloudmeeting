#pragma once
#include <QJsonObject>
#include <QtSql>

QJsonObject handleRegister(const QJsonObject& req, QSqlDatabase& db);
QJsonObject handleLogin(const QJsonObject& req, QSqlDatabase& db);
QJsonObject handleNewOrder(const QJsonObject& req, QSqlDatabase& db);
QJsonObject handleUpdateOrder(const QJsonObject& req, QSqlDatabase& db);
QJsonObject handleGetOrders(const QJsonObject& req, QSqlDatabase& db);
