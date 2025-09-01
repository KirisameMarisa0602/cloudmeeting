#include <QtCore>
#include <QtNetwork>
#include <QtSql>
#include <QJsonDocument>
#include <QJsonObject>
#include "db_bootstrap.h"
#include "server_actions.h"

static QString dbPath()
{
    return QCoreApplication::applicationDirPath() + "/cloudmeeting.db";
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // 1) 打开 & 自举数据库
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

    // 2) 启动 TCP 服务器（行分隔 JSON 协议）
    quint16 port = 5555;
    if (app.arguments().size() >= 2) {
        bool ok=false; int p = app.arguments().at(1).toInt(&ok);
        if (ok && p>0 && p<65536) port = static_cast<quint16>(p);
    }

    QTcpServer server;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]{
        while (QTcpSocket* sock = server.nextPendingConnection()) {
            sock->setTextModeEnabled(true);
            QObject::connect(sock, &QTcpSocket::readyRead, sock, [sock, &db]{
                while (sock->canReadLine()) {
                    const QByteArray line = sock->readLine().trimmed();
                    if (line.isEmpty()) continue;

                    QJsonParseError pe{};
                    const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
                    QJsonObject resp;

                    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                        resp = QJsonObject{{"ok", false}, {"msg","invalid json"}};
                    } else {
                        const QJsonObject req = doc.object();
                        const QString action = req.value("action").toString();

                        if (action == "register")            resp = handleRegister(req, db);
                        else if (action == "login")          resp = handleLogin(req, db);
                        else if (action == "new_order")      resp = handleNewOrder(req, db);
                        else if (action == "update_order")   resp = handleUpdateOrder(req, db);
                        else if (action == "get_orders")     resp = handleGetOrders(req, db);
                        else if (action == "delete_order") {
                            int id = req.value("id").toInt();
                            QSqlQuery q(db);
                            if (q.prepare("DELETE FROM orders WHERE id=?")) {
                                q.addBindValue(id);
                                bool ok = q.exec();
                                resp["ok"] = ok;
                                if (!ok) resp["msg"] = q.lastError().text();
                            } else {
                                resp = QJsonObject{{"ok", false}, {"msg","sql prepare failed"}};
                            }
                        } else {
                            resp = QJsonObject{{"ok", false}, {"msg","unknown action"}};
                        }
                    }

                    sock->write(QJsonDocument(resp).toJson(QJsonDocument::Compact));
                    sock->write("\n");
                    sock->flush();
                }
            });
            QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
        }
    });

    if (!server.listen(QHostAddress::Any, port)) {
        qCritical() << "Listen failed on port" << port << ":" << server.errorString();
        return 3;
    }
    qInfo() << "Server is listening on port" << port << "DB:" << dbPath();

    return app.exec();
}
