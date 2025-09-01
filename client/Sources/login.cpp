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
    const QString role = selectedRole();
    const QString username = ui->leUsername->text().trimmed();
    const QString password = ui->lePassword->text();

    if (role.isEmpty()) { QMessageBox::information(this, "提示", "请选择身份"); return; }
    if (username.isEmpty() || password.isEmpty()) { QMessageBox::information(this, "提示", "请输入账号和密码"); return; }

    QJsonObject rep;
    QString err;
    if (!sendRequest(QJsonObject{{"action","login"},{"role",role},{"username",username},{"password",password}}, rep, &err)) {
        QMessageBox::warning(this, "登录失败", err);
        return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "登录失败", rep.value("msg").toString("账号或密码错误"));
        return;
    }

    // 登录成功 -> 进入对应端
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
    // 打开注册页并隐藏本页；注册页关闭或发出返回登录信号时，再显示登录页
    auto* reg = new Regist();
    reg->setAttribute(Qt::WA_DeleteOnClose);
    // 将当前选择与输入预填到注册页
    const QString role = selectedRole();
    reg->preset(role, ui->leUsername->text().trimmed(), QString());

    connect(reg, &Regist::requestBackToLogin, this, [this, reg]{
        if (reg) reg->close();
        this->show();
        this->raise();
        this->activateWindow();
    });
    connect(reg, &QObject::destroyed, this, [this]{
        this->show();
        this->raise();
        this->activateWindow();
    });

    reg->show();
    this->hide();
}
