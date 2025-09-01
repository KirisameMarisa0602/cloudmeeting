#include "login.h"              // 关键：必须先包含类声明
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

Login::Login(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Login)
{
    ui->setupUi(this);

    // 主按钮用蓝色主题
    ui->btnLogin->setProperty("primary", true);

    // 初始化角色下拉
    ui->cbRole->clear();
    ui->cbRole->addItem("请选择身份"); // 0
    ui->cbRole->addItem("专家");        // 1
    ui->cbRole->addItem("工厂");        // 2
    ui->cbRole->setCurrentIndex(0);

    // 根据身份实时预览登录界面样式（不改业务逻辑）
    auto applyPreview = [this]() {
        const int idx = ui->cbRole->currentIndex();
        if (idx == 1)       Theme::applyExpertTheme(this);
        else if (idx == 2)  Theme::applyFactoryTheme(this);
        else                this->setStyleSheet(QString()); // 清空为默认
    };
    connect(ui->cbRole, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [applyPreview](int){ applyPreview(); });
    applyPreview();

    // 信号
    connect(ui->btnLogin, &QPushButton::clicked, this, &Login::on_btnLogin_clicked);
    connect(ui->btnToReg, &QPushButton::clicked, this, &Login::on_btnToReg_clicked);
}

Login::~Login()
{
    delete ui;
}

void Login::closeEvent(QCloseEvent *event)
{
    // 让登录窗口决定何时退出
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
    int nl = resp.indexOf('\n');
    if (nl >= 0) resp = resp.left(nl);
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
    const QString role     = selectedRole();                   // "expert" | "factory"
    const QString account  = ui->leUsername->text().trimmed(); // UI上显示为“账号”
    const QString password = ui->lePassword->text();

    if (role.isEmpty()) {
        QMessageBox::warning(this, "登录失败", "请选择身份");
        return;
    }
    if (account.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "登录失败", "请输入账号和密码");
        return;
    }

    // 扩展版服务端协议：login 需要 role + account + password
    QJsonObject req{
        {"action",   "login"},
        {"role",     role},
        {"account",  account},
        {"password", password}
    };

    QJsonObject rep;
    QString err;
    if (!sendRequest(req, rep, &err)) {
        QMessageBox::warning(this, "登录失败", err);
        return;
    }
    if (!rep.value("ok").toBool(false)) {
        const QString msg = rep.value("msg").toString("未知错误");
        QMessageBox::warning(this, "登录失败", msg);
        return;
    }

    // 服务端返回 username（用于展示）
    const QString username = rep.value("username").toString();

    // 记录会话并进入对应端
    if (role == "expert") {
        UserSession::expertUsername = username;
        if (!expertWin) expertWin = new ClientExpert();
        expertWin->show();
        this->hide();
    } else { // factory
        UserSession::factoryAccount  = account;
        UserSession::factoryUsername = username;
        if (!factoryWin) factoryWin = new ClientFactory();
        factoryWin->show();
        this->hide();
    }
}

void Login::on_btnToReg_clicked()
{
    // 将当前已选择的身份/账号/密码预填到注册界面
    QString prefRole;
    switch (ui->cbRole->currentIndex()) {
        case 1: prefRole = "expert"; break;
        case 2: prefRole = "factory"; break;
        default: prefRole.clear(); break;
    }
    const QString prefUser = ui->leUsername->text().trimmed();
    const QString prefPass = ui->lePassword->text();

    openRegistDialog(this, prefRole, prefUser, prefPass);
}
