#include "client_factory.h"
#include "ui_client_factory.h"
#include "comm/commwidget.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialogButtonBox>

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

extern QString g_factoryUsername;

class NewOrderDialog : public QDialog {
public:
    QLineEdit* editTitle;
    QTextEdit* editDesc;
    NewOrderDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("新建工单");
        setMinimumSize(400, 260);

        QVBoxLayout* layout = new QVBoxLayout(this);
        QLabel* labelTitle = new QLabel("工单标题：", this);
        editTitle = new QLineEdit(this);
        QLabel* labelDesc = new QLabel("工单描述：", this);
        editDesc = new QTextEdit(this);
        editDesc->setMinimumHeight(100);

        QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

        layout->addWidget(labelTitle);
        layout->addWidget(editTitle);
        layout->addWidget(labelDesc);
        layout->addWidget(editDesc);
        layout->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
};


ClientFactory::ClientFactory(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientFactory)
{
    ui->setupUi(this);

    // 实时通讯模块集成
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);

    // 选中tab时可激活通讯界面
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int idx){
        if (ui->tabWidget->widget(idx) == ui->tabRealtime) {
            commWidget_->mainWindow()->setFocus();
        }
    });

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ClientFactory::on_tabChanged);
       connect(ui->btnSearchOrder, &QPushButton::clicked, this, &ClientFactory::onSearchOrder);
       connect(ui->btnRefreshOrderStatus, &QPushButton::clicked, this, &ClientFactory::refreshOrders);
       connect(ui->btnDeleteOrder, &QPushButton::clicked, this, &ClientFactory::on_btnDeleteOrder_clicked);

       refreshOrders();
       updateTabEnabled();

}

ClientFactory::~ClientFactory()
{
    delete ui;
}

void ClientFactory::refreshOrders()
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) {
        QMessageBox::warning(this, "提示", "无法连接服务器");
        return;
    }
    QJsonObject req{{"action", "get_orders"}};
    req["role"] = "factory";
    req["username"] = g_factoryUsername;
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
    tbl->clearSelection();
}

void ClientFactory::on_btnNewOrder_clicked()
{
    NewOrderDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString title = dlg.editTitle->text().trimmed();
        QString desc = dlg.editDesc->toPlainText().trimmed();
        if (title.isEmpty()) {
            QMessageBox::warning(this, "提示", "工单标题不能为空");
            return;
        }
        sendCreateOrder(title, desc);
        QTimer::singleShot(150, this, [this]{ refreshOrders(); });
    }
}

void ClientFactory::sendCreateOrder(const QString& title, const QString& desc)
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) {
        QMessageBox::warning(this, "提示", "无法连接服务器");
        return;
    }
    QJsonObject req{
        {"action", "new_order"},
        {"title", title},
        {"desc", desc},
        {"factory_user", g_factoryUsername}
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

void ClientFactory::on_btnDeleteOrder_clicked()
{
    if (deletingOrder) return;
    deletingOrder = true;

    int row = ui->tableOrders->currentRow();
    if (row < 0 || row >= orders.size()) {
        QMessageBox::warning(this, "提示", "请选择要销毁的工单");
        deletingOrder = false;
        return;
    }
    int id = orders[row].id;
    if (QMessageBox::question(this, "确认", "确定要销毁该工单？") != QMessageBox::Yes) {
        deletingOrder = false;
        return;
    }
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) {
        QMessageBox::warning(this, "提示", "无法连接服务器");
        deletingOrder = false;
        return;
    }
    QJsonObject req{
        {"action", "delete_order"},
        {"id", id},
        {"username", g_factoryUsername}
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
        deletingOrder = false;
        return;
    }
    QTimer::singleShot(150, this, [this]{
        refreshOrders();
        deletingOrder = false;
    });
}

void ClientFactory::updateTabEnabled()
{
    ui->tabWidget->setTabEnabled(1, true);
    ui->tabWidget->setTabEnabled(3, true);
}

void ClientFactory::on_tabChanged(int idx)
{
    if (idx == 0) refreshOrders();
}

void ClientFactory::onSearchOrder()
{
    refreshOrders();
}
