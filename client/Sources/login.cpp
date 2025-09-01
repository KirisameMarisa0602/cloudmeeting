#include "login.h"
#include "ui_login.h"
#include "regist.h"
#include "client_factory.h"
#include "client_expert.h"
#include "user_session.h"
#include "theme.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QHostAddress>
#include <QTcpSocket>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QComboBox>
#include <QLineEdit>

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

Login::Login(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Login)
{
    ui->setupUi(this);

    ui->btnLogin->setProperty("primary", true);

    ui->cbRole->clear();
    ui->cbRole->addItem("请选择身份"); // 0
    ui->cbRole->addItem("专家");        // 1
    ui->cbRole->addItem("工厂");        // 2
    ui->cbRole->setCurrentIndex(0);

    auto applyPreview = [this]() {
        switch (ui->cbRole->currentIndex()) {
        case 1: Theme::applyExpertTheme(this);  break;
        case 2: Theme::applyFactoryTheme(this); break;
        default: this->setStyleSheet(QString()); break;
        }
    };
    connect(ui->cbRole, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [applyPreview](int){ applyPreview(); });
    applyPreview();

    // 重要：不要再手动 connect(btnLogin/btnToReg, clicked, ...)
    // 本类使用 on_<object>_<signal> 自动连接，避免重复触发
}

Login::~Login()
{
    delete ui;
}

void Login::closeEvent(QCloseEvent *event)
{
    QCoreApplication::quit();
    QWidget::closeEvent(event);
}

QString Login::selectedRole() const
{
    switch (ui->cbRole->currentIndex()) {
    case 1: return "expert";
    case 2: return "factory";
    default: return "";
    }
}

bool Login::sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg)
{
    QTcpSocket sock;
    sock.connectToHost(QHostAddress(QString::fromLatin1(SERVER_HOST)), SERVER_PORT);
    if (!sock.waitForConnected(3000)) {
        if (errMsg) *errMsg = "服务器连接失败";
        return false;
    }
    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    if (sock.write(line) == -1 || !sock.waitForBytesWritten(2000)) {
        if (errMsg) *errMsg = "请求发送失败";
        return false;
    }
    if (!sock.waitForReadyRead(5000)) {
        if (errMsg) *errMsg = "服务器无响应";
        return false;
    }
    QByteArray resp = sock.readAll();
    if (int nl = resp.indexOf('\n'); nl >= 0) resp = resp.left(nl);

    QJsonParseError pe{};
    QJsonDocument rdoc = QJsonDocument::fromJson(resp, &pe);
    if (pe.error != QJsonParseError::NoError || !rdoc.isObject()) {
        if (errMsg) *errMsg = "响应解析失败";
        return false;
    }
    reply = rdoc.object();
    return true;
}

void Login::on_btnLogin_clicked()
{
    const QString role = selectedRole();
    const QString username = ui->leUsername->text().trimmed();
    const QString password = ui->lePassword->text();

    if (role.isEmpty()) { QMessageBox::information(this, "提示", "请选择身份"); return; }
    if (username.isEmpty() || password.isEmpty()) { QMessageBox::information(this, "提示", "请输入账号和密码"); return; }

    QJsonObject rep; QString err;
    if (!sendRequest(QJsonObject{{"action","login"},{"role",role},{"username",username},{"password",password}}, rep, &err)) {
        QMessageBox::warning(this, "登录失败", err);
        return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "登录失败", rep.value("msg").toString("账号或密码错误"));
        return;
    }

    if (role == "expert") {
        UserSession::expertUsername = username;
        if (!expertWin) expertWin = new ClientExpert;
        expertWin->show();
    } else {
        UserSession::factoryUsername = username;
        if (!factoryWin) factoryWin = new ClientFactory;
        factoryWin->show();
    }
    this->hide();
}

void Login::on_btnToReg_clicked()
{
    // 防重复：若已经有注册窗口，只激活它
    if (regWin) {
        regWin->raise();
        regWin->activateWindow();
        return;
    }

    regWin = new Regist();
    regWin->setAttribute(Qt::WA_DeleteOnClose);

    // 将当前选择与用户名预填到注册页
    const QString role = selectedRole();
    regWin->preset(role, ui->leUsername->text().trimmed(), QString());

    // 注册页返回/关闭 -> 回到登录页，并清空指针
    connect(regWin, &Regist::requestBackToLogin, this, [this]{
        if (regWin) regWin->close();
    });
    connect(regWin, &QObject::destroyed, this, [this]{
        regWin = nullptr;
        this->show(); this->raise(); this->activateWindow();
    });

    regWin->show();
    this->hide();
}
