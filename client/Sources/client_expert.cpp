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
#include <QTableWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QBrush>
#include <QColor>

QString g_factoryUsername;
QString g_expertUsername;

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

static QColor statusColor(const QString& s) {
    if (s == QStringLiteral("已接受")) return QColor(59, 130, 246);  // 蓝
    if (s == QStringLiteral("已拒绝")) return QColor(220, 38, 38);   // 红
    return QColor(234, 179, 8);                                      // 橙
}

ClientExpert::ClientExpert(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientExpert)
{
    ui->setupUi(this);

    // 实时通讯模块集成
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

    // 角色化 UI 与表格装饰
    applyRoleUi();
    decorateOrdersTable();

    // 状态筛选（保留）
    ui->comboBoxStatus->clear();
    ui->comboBoxStatus->addItem("全部");
    ui->comboBoxStatus->addItem("待处理");
    ui->comboBoxStatus->addItem("已接受");
    ui->comboBoxStatus->addItem("已拒绝");

    // 双击查看详情
    connect(ui->tableOrders, &QTableWidget::cellDoubleClicked,
            this, &ClientExpert::onOrderDoubleClicked);

    refreshOrders();
    updateTabEnabled();
}

ClientExpert::~ClientExpert()
{
    delete ui;
}

void ClientExpert::applyRoleUi()
{
    setWindowTitle(QStringLiteral("专家端 | 智能协同云会议"));

    if (ui->tabWidget && ui->tabWidget->count() > 0) {
        ui->tabWidget->setTabText(0, QStringLiteral("专家端 • 工单中心"));
    }

    if (ui->lineEditKeyword) {
        ui->lineEditKeyword->setPlaceholderText(QStringLiteral("按工单号/标题关键词搜索…"));
    }

    if (ui->btnAccept) ui->btnAccept->setToolTip(QStringLiteral("接受所选工单并进入协作"));
    if (ui->btnReject) ui->btnReject->setToolTip(QStringLiteral("拒绝所选工单"));
    if (ui->btnRefreshOrderStatus) ui->btnRefreshOrderStatus->setToolTip(QStringLiteral("刷新工单列表"));
    if (ui->btnSearchOrder) ui->btnSearchOrder->setToolTip(QStringLiteral("按关键词与状态筛选"));

    // 表头&表格边框的轻微强化（与主题叠加）
    this->setStyleSheet(this->styleSheet() +
        " QHeaderView::section { background: rgba(59,130,246,0.18); }"
        " QTableWidget { border: 1px solid rgba(59,130,246,0.45); }"
    );
}

void ClientExpert::decorateOrdersTable()
{
    auto* tbl = ui->tableOrders;
    if (!tbl) return;
    tbl->setAlternatingRowColors(true);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setSelectionMode(QAbstractItemView::SingleSelection);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->verticalHeader()->setVisible(false);
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setShowGrid(true);
}

void ClientExpert::setJoinedOrder(bool joined)
{
    joinedOrder = joined;
    updateTabEnabled();
}

void ClientExpert::updateTabEnabled()
{
    // 改为“始终启用”Tab，通过切换时拦截来提示更友好
    ui->tabWidget->setTabEnabled(1, true);
    ui->tabWidget->setTabEnabled(3, true);
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
        auto* itemId = new QTableWidgetItem(QString::number(od.id));
        auto* itemTitle = new QTableWidgetItem(od.title);
        auto* itemDesc = new QTableWidgetItem(od.desc);
        auto* itemStatus = new QTableWidgetItem(od.status);

        const QColor fg = statusColor(od.status);
        const QColor bg = QColor(fg.red(), fg.green(), fg.blue(), 28); // 轻背景

        itemId->setForeground(QBrush(fg));
        itemTitle->setForeground(QBrush(fg));
        itemDesc->setForeground(QBrush(fg));
        itemStatus->setForeground(QBrush(fg));

        itemId->setBackground(QBrush(bg));
        itemTitle->setBackground(QBrush(bg));
        itemDesc->setBackground(QBrush(bg));
        itemStatus->setBackground(QBrush(bg));

        tbl->setItem(i, 0, itemId);
        tbl->setItem(i, 1, itemTitle);
        tbl->setItem(i, 2, itemDesc);
        tbl->setItem(i, 3, itemStatus);
    }
    tbl->resizeColumnsToContents();
    tbl->clearSelection();
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
    // idx==0 工单中心；1/3 为需要加入工单后才可交互的页面
    if ((idx == 1 || idx == 3) && !joinedOrder) {
        QMessageBox::information(this, "提示", "暂无待处理工单");
        ui->tabWidget->setCurrentIndex(0);
        return;
    }
    if (idx == 0) refreshOrders();
}

void ClientExpert::onSearchOrder()
{
    refreshOrders();
}

void ClientExpert::onOrderDoubleClicked(int row, int /*column*/)
{
    if (row < 0 || row >= orders.size()) return;
    showOrderDetailsDialog(orders[row]);
}

void ClientExpert::showOrderDetailsDialog(const OrderInfo& od)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("工单详情（专家端）"));
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    auto mk = [](const QString& k, const QString& v){
        QWidget* w = new QWidget;
        QHBoxLayout* h = new QHBoxLayout(w);
        h->setContentsMargins(0,0,0,0);
        QLabel* lk = new QLabel("<b>" + k + "</b>");
        QLabel* lv = new QLabel(v);
        lv->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(lk);
        h->addWidget(lv, 1);
        return w;
    };
    layout->addWidget(mk("工单号：", QString::number(od.id)));
    layout->addWidget(mk("标题：", od.title));
    layout->addWidget(mk("状态：", od.status));
    QLabel* desc = new QLabel("<b>描述：</b>");
    QLabel* dval = new QLabel(od.desc);
    dval->setWordWrap(true);
    dval->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(desc);
    layout->addWidget(dval);
    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    layout->addWidget(box);
    dlg.resize(520, 320);
    dlg.exec();
}
