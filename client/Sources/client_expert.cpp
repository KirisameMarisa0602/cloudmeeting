#include "client_expert.h"
#include "ui_client_expert.h"
#include "comm/commwidget.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QTimer>
#include <QString>

QString g_factoryUsername;
QString g_expertUsername;

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

ClientExpert::ClientExpert(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientExpert)
{
    ui->setupUi(this);

    // 实时通讯模块集成（同理，可以和ClientFactory一致）
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int idx){
        if (ui->tabWidget->widget(idx) == ui->tabRealtime) {
            commWidget_->mainWindow()->setFocus();
        }
    });

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ClientExpert::on_tabChanged);
      connect(ui->btnAccept, &QPushButton::clicked, this, &ClientExpert::on_btnAccept_clicked);
      connect(ui->btnReject, &QPushButton::clicked, this, &ClientExpert::on_btnReject_clicked);
      connect(ui->btnRefreshOrderStatus, &QPushButton::clicked, this, &ClientExpert::refreshOrders);
      connect(ui->btnSearchOrder, &QPushButton::clicked, this, &ClientExpert::onSearchOrder);

      ui->comboBoxStatus->clear();
      ui->comboBoxStatus->addItem("全部");
      ui->comboBoxStatus->addItem("待处理");
      ui->comboBoxStatus->addItem("已接受");
      ui->comboBoxStatus->addItem("已拒绝");

      refreshOrders();
      updateTabEnabled();

}

ClientExpert::~ClientExpert()
{
    delete ui;
}

void ClientExpert::setJoinedOrder(bool joined)
{
    joinedOrder = joined;
    updateTabEnabled();
}

void ClientExpert::updateTabEnabled()
{
    ui->tabWidget->setTabEnabled(1, joinedOrder);
    ui->tabWidget->setTabEnabled(3, joinedOrder);
}

void ClientExpert::refreshOrders()
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) {
        QMessageBox::warning(this, "提示", "无法连接服务器");
        return;
    }
    QJsonObject req{{"action", "get_orders"}};
    req["role"] = "expert";
    req["username"] = g_expertUsername;
    QString keyword = ui->lineEditKeyword->text().trimmed();
    if (!keyword.isEmpty()) req["keyword"] = keyword;
    QString status = ui->comboBoxStatus->currentText();
    if (status != "全部") req["status"] = status;

    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForBytesWritten(1000);
    sock.waitForReadyRead(2000);
    QByteArray resp = sock.readAll();
    int nl = resp.indexOf('\n');
    if (nl >= 0) resp = resp.left(nl);
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject() || !doc.object().value("ok").toBool()) {
        QMessageBox::warning(this, "提示", "服务器响应异常");
        return;
    }
    orders.clear();
    QJsonArray arr = doc.object().value("orders").toArray();
    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        orders.append(OrderInfo{
            o.value("id").toInt(), o.value("title").toString(),
            o.value("desc").toString(), o.value("status").toString()
        });
    }
    auto* tbl = ui->tableOrders;
    tbl->clear();
    tbl->setColumnCount(4);
    tbl->setRowCount(orders.size());
    QStringList headers{"工单号", "标题", "描述", "状态"};
    tbl->setHorizontalHeaderLabels(headers);
    for (int i = 0; i < orders.size(); ++i) {
        const auto& od = orders[i];
        tbl->setItem(i, 0, new QTableWidgetItem(QString::number(od.id)));
        tbl->setItem(i, 1, new QTableWidgetItem(od.title));
        tbl->setItem(i, 2, new QTableWidgetItem(od.desc));
        tbl->setItem(i, 3, new QTableWidgetItem(od.status));
    }
    tbl->resizeColumnsToContents();
}

void ClientExpert::on_btnAccept_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0 || row >= orders.size()) {
        QMessageBox::warning(this, "提示", "请选择一个工单");
        return;
    }
    int id = orders[row].id;
    sendUpdateOrder(id, "已接受");
    setJoinedOrder(true);
    QTimer::singleShot(150, this, [this]{ refreshOrders(); });
}

void ClientExpert::on_btnReject_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0 || row >= orders.size()) {
        QMessageBox::warning(this, "提示", "请选择一个工单");
        return;
    }
    int id = orders[row].id;
    sendUpdateOrder(id, "已拒绝");
    setJoinedOrder(false);
    QTimer::singleShot(150, this, [this]{ refreshOrders(); });
}

void ClientExpert::sendUpdateOrder(int orderId, const QString& status)
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) {
        QMessageBox::warning(this, "提示", "无法连接服务器");
        return;
    }
    QJsonObject req{
        {"action", "update_order"},
        {"id", orderId},
        {"status", status}
    };
    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForBytesWritten(1000);
    sock.waitForReadyRead(2000);
    QByteArray resp = sock.readAll();
    int nl = resp.indexOf('\n');
    if (nl >= 0) resp = resp.left(nl);
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject() || !doc.object().value("ok").toBool()) {
        QMessageBox::warning(this, "提示", "服务器响应异常");
    }
}

void ClientExpert::on_tabChanged(int idx)
{
    if (idx == 0) refreshOrders();
}

void ClientExpert::onSearchOrder()
{
    refreshOrders();
}

