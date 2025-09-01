#include "register_dialog.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

static bool doRegister(const QString& role, const QString& account, const QString& username, const QString& password, QString* err)
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) { if (err) *err = "无法连接服务器"; return false; }

    QJsonObject req{
        {"action", "register"},
        {"role", (role.contains("厂") ? "factory" : "expert")},
        {"account", account},
        {"password", password},
        {"username", username}
    };
    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForReadyRead(3000);
    QByteArray resp = sock.readAll();
    int nl = resp.indexOf('\n'); if (nl >= 0) resp = resp.left(nl);
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject() || !doc.object().value("ok").toBool()) {
        if (err) *err = doc.isObject() ? doc.object().value("msg").toString() : "注册失败";
        return false;
    }
    return true;
}

// 供登录窗调用
bool OpenAndSubmitRegister(QWidget* parent)
{
    RegisterDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted) return false;
    const QString role = dlg.comboRole->currentText();
    const QString account  = dlg.editAccount->text().trimmed();
    const QString username = dlg.editUsername->text().trimmed();
    const QString password = dlg.editPassword->text();
    if (account.isEmpty() || username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(parent, "提示", "账号/用户名称/密码均不能为空");
        return false;
    }
    QString err;
    if (!doRegister(role, account, username, password, &err)) {
        QMessageBox::warning(parent, "提示", err);
        return false;
    }
    QMessageBox::information(parent, "提示", "注册成功，请使用账号登录");
    return true;
}
