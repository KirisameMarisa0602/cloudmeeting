#include "regist.h"
#include "ui_regist.h"
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>
#include <QHostAddress>
#include <QComboBox>
#include <QLineEdit>
#include "theme.h"

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

Regist::Regist(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Regist)
{
    ui->setupUi(this);

    setWindowFlag(Qt::Window, true);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->btnRegister->setProperty("primary", true);
    ui->cbRole->clear();
    ui->cbRole->addItem("请选择身份"); // 0
    ui->cbRole->addItem("专家");        // 1
    ui->cbRole->addItem("工厂");        // 2
    ui->cbRole->setCurrentIndex(0);

    // 根据身份实时预览注册界面样式（不改业务逻辑）
    auto applyPreview = [this]() {
        const int idx = ui->cbRole->currentIndex();
        if (idx == 1)       Theme::applyExpertTheme(this);
        else if (idx == 2)  Theme::applyFactoryTheme(this);
        else                this->setStyleSheet(QString());
    };
    connect(ui->cbRole, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [applyPreview](int){ applyPreview(); });
    applyPreview();

    // 事件
    connect(ui->btnRegister, &QPushButton::clicked, this, &Regist::on_btnRegister_clicked);
    // 注意：UI 中按钮对象名是 btnBack（不是 btnBackLogin）
    connect(ui->btnBack, &QPushButton::clicked, this, &Regist::on_btnBackLogin_clicked);
}

Regist::~Regist()
{
    delete ui;
}

void Regist::preset(const QString &role, const QString &user, const QString &pass)
{
    if (role == "expert") ui->cbRole->setCurrentIndex(1);
    else if (role == "factory") ui->cbRole->setCurrentIndex(2);
    else ui->cbRole->setCurrentIndex(0);

    ui->leUsername->setText(user);
    ui->lePassword->setText(pass);
    ui->leConfirm->clear();
}

QString Regist::selectedRole() const
{
    switch (ui->cbRole->currentIndex()) {
    case 1: return "expert";
    case 2: return "factory";
    default: return "";
    }
}

bool Regist::sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg)
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

void Regist::on_btnRegister_clicked()
{
    const QString role = selectedRole();
    const QString username = ui->leUsername->text().trimmed();
    const QString pass = ui->lePassword->text();
    const QString confirm = ui->leConfirm->text();

    if (role.isEmpty()) { QMessageBox::information(this, "提示", "请选择身份"); return; }
    if (username.isEmpty() || pass.isEmpty()) { QMessageBox::information(this, "提示", "请输入账号和密码"); return; }
    if (pass != confirm) { QMessageBox::information(this, "提示", "两次密码不一致"); return; }

    QJsonObject rep;
    QString err;
    if (!sendRequest(QJsonObject{{"action","register"},{"role",role},{"username",username},{"password",pass}}, rep, &err)) {
        QMessageBox::warning(this, "注册失败", err);
        return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "注册失败", rep.value("msg").toString("未知错误"));
        return;
    }

    QMessageBox::information(this, "成功", "注册成功，请使用该账号登录");
    emit requestBackToLogin();
    close();
}

void Regist::on_btnBackLogin_clicked()
{
    emit requestBackToLogin();
    close();
}
