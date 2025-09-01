#include "client_expert.h"
#include "ui_client_expert.h"
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
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QBrush>
#include <QColor>
#include <QPushButton>
#include <QShortcut>

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

static QColor statusColor(const QString& s) {
    if (s == QStringLiteral("已接受")) return QColor(59, 130, 246);
    if (s == QStringLiteral("已拒绝")) return QColor(220, 38, 38);
    return QColor(234, 179, 8);
}

// 发送一条行分隔 JSON 请求到服务器并返回回复
static bool sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg = nullptr)
{
    QTcpSocket sock;
    sock.connectToHost(QHostAddress(QString::fromLatin1(SERVER_HOST)), SERVER_PORT);
    if (!sock.waitForConnected(3000)) { if (errMsg) *errMsg = "服务器连接失败"; return false; }
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    if (sock.write(line) == -1 || !sock.waitForBytesWritten(2000)) { if (errMsg) *errMsg = "请求发送失败"; return false; }
    if (!sock.waitForReadyRead(5000)) { if (errMsg) *errMsg = "服务器无响应"; return false; }
    QByteArray resp = sock.readAll();
    int nl = resp.indexOf('\n');
    if (nl >= 0) resp = resp.left(nl);
    QJsonParseError pe{};
    QJsonDocument rdoc = QJsonDocument::fromJson(resp, &pe);
    if (pe.error != QJsonParseError::NoError || !rdoc.isObject()) { if (errMsg) *errMsg = "响应解析失败"; return false; }
    reply = rdoc.object();
    return true;
}

ClientExpert::ClientExpert(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientExpert)
{
    ui->setupUi(this);
    Theme::applyExpertTheme(this);
    applyRoleUi();

    // 仅保留右侧聊天：CommWidget 内部已隐藏 MainWindow 的聊天控件
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);

    // 进入统一示例房间（可按工单进入时改为 order-<id>）
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT, "Room1", UserSession::expertUsername);

    // Tab 角落：左侧显示当前用户名称，右侧“更改账号”
    labUserNameCorner_ = new QLabel(QStringLiteral("用户：") + UserSession::expertUsername, ui->tabWidget);
    labUserNameCorner_->setStyleSheet("padding:4px 10px; color:#e5edff;");
    ui->tabWidget->setCornerWidget(labUserNameCorner_, Qt::TopLeftCorner);

    QPushButton* btnSwitch = new QPushButton(QStringLiteral("更改账号"), ui->tabWidget);
    btnSwitch->setToolTip(QStringLiteral("返回登录页以更换账号（快捷键：Ctrl+L）"));
    btnSwitch->setCursor(Qt::PointingHandCursor);
    btnSwitch->setObjectName("btnSwitchAccount");
    btnSwitch->setStyleSheet(
        "QPushButton#btnSwitchAccount { background: rgba(59,130,246,0.22); border: 1px solid #3b82f6; color: #e6f0ff; border-radius: 6px; padding: 4px 10px; }"
        "QPushButton#btnSwitchAccount:hover { background: rgba(147,197,253,0.35); }"
    );
    ui->tabWidget->setCornerWidget(btnSwitch, Qt::TopRightCorner);
    connect(btnSwitch, &QPushButton::clicked, this, [this]{ logoutToLogin(); });

    auto* sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(sc, &QShortcut::activated, this, &ClientExpert::logoutToLogin);

    // 订单表格
    decorateOrdersTable();

    // 信号
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ClientExpert::on_tabChanged);
    connect(ui->btnAccept, &QPushButton::clicked, this, &ClientExpert::on_btnAccept_clicked);
    connect(ui->btnReject, &QPushButton::clicked, this, &ClientExpert::on_btnReject_clicked);
    connect(ui->btnSearchOrder, &QPushButton::clicked, this, &ClientExpert::onSearchOrder);
    // 刷新按钮（如果 UI 中不存在则忽略）
    if (auto btn = this->findChild<QPushButton*>("btnRefreshOrderStatus"))
        connect(btn, &QPushButton::clicked, this, &ClientExpert::onSearchOrder);

    // 初始数据
    refreshOrders();
    updateTabEnabled();
}

ClientExpert::~ClientExpert()
{
    delete ui;
}

void ClientExpert::applyRoleUi()
{
    // 在此可补充专家端特定样式；保持空实现也可
}

void ClientExpert::decorateOrdersTable()
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
    connect(t, &QTableWidget::cellDoubleClicked, this, &ClientExpert::onOrderDoubleClicked);
}

void ClientExpert::setJoinedOrder(bool joined)
{
    joinedOrder = joined;
    updateTabEnabled();
}

void ClientExpert::updateTabEnabled()
{
    // 当选择了一行才允许接收/拒绝
    bool hasSelection = ui->tableOrders->currentRow() >= 0;
    ui->btnAccept->setEnabled(hasSelection);
    ui->btnReject->setEnabled(hasSelection);
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

void ClientExpert::refreshOrders()
{
    QJsonObject rep;
    QString err;
    if (!sendRequest(QJsonObject{{"action","get_orders"}}, rep, &err)) {
        QMessageBox::warning(this, "获取工单失败", err);
        return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "获取工单失败", rep.value("msg").toString("未知错误"));
        return;
    }
    orders.clear();
    const QJsonArray arr = rep.value("orders").toArray();
    orders.reserve(arr.size());
    for (const auto& v : arr) orders.push_back(orderFromJson(v.toObject()));

    // 根据筛选状态（如果存在）过滤后填充
    QString selStatus;
    if (ui->comboBoxStatus) selStatus = ui->comboBoxStatus->currentText();
    auto* t = ui->tableOrders;
    t->setRowCount(0);
    for (const auto& od : orders) {
        if (!selStatus.isEmpty() && selStatus != "全部" && od.status != selStatus) continue;

        const int r = t->rowCount();
        t->insertRow(r);

        auto idItem = new QTableWidgetItem(QString::number(od.id));
        idItem->setData(Qt::UserRole, od.id);
        auto titleItem = new QTableWidgetItem(od.title);
        auto descItem = new QTableWidgetItem(od.desc);
        auto statusItem = new QTableWidgetItem(od.status);
        statusItem->setForeground(statusColor(od.status));
        auto pubItem = new QTableWidgetItem(od.publisher);
        auto accItem = new QTableWidgetItem(od.accepter);

        t->setItem(r, 0, idItem);
        t->setItem(r, 1, titleItem);
        t->setItem(r, 2, descItem);
        t->setItem(r, 3, statusItem);
        t->setItem(r, 4, pubItem);
        t->setItem(r, 5, accItem);
    }
    updateTabEnabled();
}

void ClientExpert::sendUpdateOrder(int orderId, const QString& status)
{
    QJsonObject rep;
    QString err;
    QJsonObject req{
        {"action","update_order"},
        {"id", orderId},
        {"status", status},
        {"accepter", UserSession::expertUsername}
    };
    if (!sendRequest(req, rep, &err)) {
        QMessageBox::warning(this, "更新工单失败", err);
        return;
    }
    if (!rep.value("ok").toBool()) {
        QMessageBox::warning(this, "更新工单失败", rep.value("msg").toString("未知错误"));
        return;
    }
    refreshOrders();
}

void ClientExpert::on_btnAccept_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0) { QMessageBox::information(this, "提示", "请先选择一条工单"); return; }
    int id = ui->tableOrders->item(row, 0)->data(Qt::UserRole).toInt();
    sendUpdateOrder(id, QStringLiteral("已接受"));
}

void ClientExpert::on_btnReject_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0) { QMessageBox::information(this, "提示", "请先选择一条工单"); return; }
    int id = ui->tableOrders->item(row, 0)->data(Qt::UserRole).toInt();
    sendUpdateOrder(id, QStringLiteral("已拒绝"));
}

void ClientExpert::on_tabChanged(int /*idx*/)
{
    updateTabEnabled();
}

void ClientExpert::onSearchOrder()
{
    // 重新拉取并根据 UI 状态进行过滤填充
    refreshOrders();
}

void ClientExpert::showOrderDetailsDialog(const OrderInfo& od)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString("工单详情 #%1").arg(od.id));
    QVBoxLayout* lay = new QVBoxLayout(&dlg);
    auto addRow = [&](const QString& k, const QString& v){
        QHBoxLayout* hl = new QHBoxLayout;
        QLabel* lk = new QLabel(k + "：", &dlg);
        lk->setMinimumWidth(70);
        QLabel* lv = new QLabel(v, &dlg);
        lv->setTextInteractionFlags(Qt::TextSelectableByMouse);
        hl->addWidget(lk); hl->addWidget(lv, 1);
        lay->addLayout(hl);
    };
    addRow("标题", od.title);
    addRow("状态", od.status);
    addRow("发布者", od.publisher);
    addRow("接受者", od.accepter.isEmpty() ? "-" : od.accepter);
    QLabel* ldesc = new QLabel("描述：", &dlg);
    QTextBrowser* tb = new QTextBrowser(&dlg);
    tb->setText(od.desc);
    lay->addWidget(ldesc);
    lay->addWidget(tb, 1);
    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    lay->addWidget(box);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    dlg.resize(480, 360);
    dlg.exec();
}

void ClientExpert::onOrderDoubleClicked(int row, int /*column*/)
{
    if (row < 0) return;
    int id = ui->tableOrders->item(row, 0)->data(Qt::UserRole).toInt();
    for (const auto& od : orders) {
        if (od.id == id) { showOrderDetailsDialog(od); break; }
    }
}

void ClientExpert::logoutToLogin()
{
    this->hide();
    auto* login = new Login();
    login->show();
}
