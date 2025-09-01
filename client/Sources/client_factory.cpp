#include "client_factory.h"
#include "ui_client_factory.h"
#include "comm/commwidget.h"
#include "theme.h"
#include "user_session.h"
#include "login.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QTimer>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QPushButton>
#include <QShortcut>

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

static QColor statusColor(const QString& s) {
    if (s == QStringLiteral("已接受")) return QColor(34, 197, 94);
    if (s == QStringLiteral("已拒绝")) return QColor(220, 38, 38);
    return QColor(234, 179, 8);
}

class NewOrderDialog : public QDialog {
public:
    QLineEdit* editTitle;
    QTextEdit* editDesc;
    NewOrderDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("新建工单");
        setMinimumSize(420, 280);
        QVBoxLayout* layout = new QVBoxLayout(this);
        QLabel* labelTitle = new QLabel("工单标题：", this);
        editTitle = new QLineEdit(this);
        QLabel* labelDesc = new QLabel("工单描述：", this);
        editDesc = new QTextEdit(this);
        editDesc->setMinimumHeight(110);
        QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(labelTitle); layout->addWidget(editTitle);
        layout->addWidget(labelDesc);  layout->addWidget(editDesc);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
};

// 简单请求助手
static bool sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg = nullptr)
{
    QTcpSocket sock;
    sock.connectToHost(QHostAddress(QString::fromLatin1(SERVER_HOST)), SERVER_PORT);
    if (!sock.waitForConnected(3000)) { if (errMsg) *errMsg = "服务器连接失败"; return false; }
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    if (sock.write(line) == -1 || !sock.waitForBytesWritten(2000)) { if (errMsg) *errMsg = "请求发送失败"; return false; }
    if (!sock.waitForReadyRead(5000)) { if (errMsg) *errMsg = "服务器无响应"; return false; }
    QByteArray resp = sock.readAll();
    if (int nl = resp.indexOf('\n'); nl >= 0) resp = resp.left(nl);
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(resp, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) { if (errMsg) *errMsg = "响应解析失败"; return false; }
    reply = doc.object();
    return true;
}

static OrderInfo orderFromJson(const QJsonObject& o)
{
    OrderInfo od;
    od.id = o.value("id").toInt();
    od.title = o.value("title").toString();
    od.desc = o.value("desc").toString();
    od.status = o.value("status").toString();
    od.publisher = o.value("publisher").toString();
    od.accepter = o.value("accepter").toString();
    return od;
}

ClientFactory::ClientFactory(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientFactory)
{
    ui->setupUi(this);
    Theme::applyFactoryTheme(this);
    applyRoleUi();

    // 通讯区域（右侧在线聊天）
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT, "Room1", UserSession::factoryUsername);

    // 顶部角落标签与切账号按钮
    labUserNameCorner_ = new QLabel(QStringLiteral("用户：") + UserSession::factoryUsername, ui->tabWidget);
    labUserNameCorner_->setStyleSheet("padding:4px 10px; color:#e8ffee;");
    ui->tabWidget->setCornerWidget(labUserNameCorner_, Qt::TopLeftCorner);

    QPushButton* btnSwitch = new QPushButton(QStringLiteral("更改账号"), ui->tabWidget);
    btnSwitch->setToolTip(QStringLiteral("返回登录页以更换账号（快捷键：Ctrl+L）"));
    btnSwitch->setCursor(Qt::PointingHandCursor);
    btnSwitch->setObjectName("btnSwitchAccount");
    ui->tabWidget->setCornerWidget(btnSwitch, Qt::TopRightCorner);
    connect(btnSwitch, &QPushButton::clicked, this, [this]{ logoutToLogin(); });

    auto* sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(sc, &QShortcut::activated, this, &ClientFactory::logoutToLogin);

    decorateOrdersTable();

    connect(ui->btnNewOrder,    &QPushButton::clicked, this, &ClientFactory::on_btnNewOrder_clicked);
    connect(ui->btnDeleteOrder, &QPushButton::clicked, this, &ClientFactory::on_btnDeleteOrder_clicked);
    connect(ui->tabWidget,      &QTabWidget::currentChanged, this, &ClientFactory::on_tabChanged);
    connect(ui->btnSearchOrder, &QPushButton::clicked, this, &ClientFactory::onSearchOrder);
    if (auto btn = this->findChild<QPushButton*>("btnRefreshOrderStatus"))
        connect(btn, &QPushButton::clicked, this, &ClientFactory::onSearchOrder);

    refreshOrders();
    updateTabEnabled();
}

ClientFactory::~ClientFactory() { delete ui; }

void ClientFactory::applyRoleUi() {}

void ClientFactory::decorateOrdersTable()
{
    auto* t = ui->tableOrders;
    t->clear();
    t->setColumnCount(6);
    t->setHorizontalHeaderLabels(QStringList() << "ID" << "标题" << "描述" << "状态" << "发布者" << "接受者");
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(t, &QTableWidget::cellDoubleClicked, this, &ClientFactory::onOrderDoubleClicked);
}

void ClientFactory::refreshOrders()
{
    QJsonObject rep; QString err;
    if (!sendRequest(QJsonObject{{"action","get_orders"}}, rep, &err)) {
        QMessageBox::warning(this, "获取工单失败", err); return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "获取工单失败", rep.value("msg").toString("未知错误"));
        return;
    }
    orders.clear();
    const QJsonArray arr = rep.value("orders").toArray();
    for (const auto& v : arr) orders.push_back(orderFromJson(v.toObject()));

    QString selStatus; if (ui->comboBoxStatus) selStatus = ui->comboBoxStatus->currentText();

    auto* t = ui->tableOrders;
    t->setRowCount(0);
    for (const auto& od : orders) {
        if (!selStatus.isEmpty() && selStatus != "全部" && od.status != selStatus) continue;
        int r = t->rowCount(); t->insertRow(r);
        auto idItem = new QTableWidgetItem(QString::number(od.id)); idItem->setData(Qt::UserRole, od.id);
        t->setItem(r,0,idItem);
        t->setItem(r,1,new QTableWidgetItem(od.title));
        t->setItem(r,2,new QTableWidgetItem(od.desc));
        auto statusItem = new QTableWidgetItem(od.status); statusItem->setForeground(statusColor(od.status));
        t->setItem(r,3,statusItem);
        t->setItem(r,4,new QTableWidgetItem(od.publisher));
        t->setItem(r,5,new QTableWidgetItem(od.accepter));
    }
    updateTabEnabled();
}

void ClientFactory::sendCreateOrder(const QString& title, const QString& desc)
{
    QJsonObject rep; QString err;
    QJsonObject req{
        {"action","new_order"},
        {"title", title},
        {"desc",  desc},
        {"factory_user", UserSession::factoryUsername} // 与服务器对齐
    };
    if (!sendRequest(req, rep, &err)) {
        QMessageBox::warning(this, "发布工单失败", err);
        return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "发布工单失败", rep.value("msg").toString("未知错误"));
        return;
    }
    refreshOrders();
}

void ClientFactory::updateTabEnabled()
{
    bool hasSelection = ui->tableOrders->currentRow() >= 0;
    ui->btnDeleteOrder->setEnabled(hasSelection);
}

void ClientFactory::on_btnNewOrder_clicked()
{
    NewOrderDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString title = dlg.editTitle->text().trimmed();
    const QString desc  = dlg.editDesc->toPlainText().trimmed();
    if (title.isEmpty() || desc.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入标题和描述");
        return;
    }
    sendCreateOrder(title, desc);
}

void ClientFactory::on_btnDeleteOrder_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0) { QMessageBox::information(this, "提示", "请选择一条工单"); return; }
    int id = ui->tableOrders->item(row,0)->data(Qt::UserRole).toInt();

    if (QMessageBox::question(this, "确认", QString("确认销毁工单 #%1 ？").arg(id)) != QMessageBox::Yes) return;

    QJsonObject rep; QString err;
    if (!sendRequest(QJsonObject{{"action","delete_order"},{"id",id},{"username", UserSession::factoryUsername}}, rep, &err)) {
        QMessageBox::warning(this, "销毁工单失败", err);
        return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "销毁工单失败", rep.value("msg").toString("未知错误"));
        return;
    }
    refreshOrders();
}

void ClientFactory::on_tabChanged(int) { updateTabEnabled(); }
void ClientFactory::onSearchOrder() { refreshOrders(); }

void ClientFactory::onOrderDoubleClicked(int row, int)
{
    if (row < 0) return;
    int id = ui->tableOrders->item(row,0)->data(Qt::UserRole).toInt();
    for (const auto& od : orders) {
        if (od.id == id) {
            QDialog dlg(this);
            dlg.setWindowTitle(QString("工单详情 #%1").arg(od.id));
            QVBoxLayout* lay = new QVBoxLayout(&dlg);
            auto addRow = [&](const QString& k, const QString& v){
                QHBoxLayout* hl = new QHBoxLayout;
                QLabel* lk = new QLabel(k + "：", &dlg); lk->setMinimumWidth(70);
                QLabel* lv = new QLabel(v, &dlg); lv->setTextInteractionFlags(Qt::TextSelectableByMouse);
                hl->addWidget(lk); hl->addWidget(lv,1); lay->addLayout(hl);
            };
            addRow("标题", od.title);
            addRow("状态", od.status);
            addRow("发布者", od.publisher);
            addRow("接受者", od.accepter.isEmpty() ? "-" : od.accepter);
            QLabel* ldesc = new QLabel("描述：", &dlg);
            QTextBrowser* tb = new QTextBrowser(&dlg); tb->setText(od.desc);
            lay->addWidget(ldesc); lay->addWidget(tb,1);
            QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
            lay->addWidget(box);
            QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            dlg.resize(480,360);
            dlg.exec();
            break;
        }
    }
}

void ClientFactory::logoutToLogin()
{
    this->hide();
    auto* login = new Login();
    login->show();
}
