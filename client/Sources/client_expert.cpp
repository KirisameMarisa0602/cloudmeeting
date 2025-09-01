#include "client_expert.h"
#include "ui_client_expert.h"
#include "comm/commwidget.h"
#include "theme.h"
#include "login.h"

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
#include <QPushButton>
#include <QShortcut>

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

    // 应用专家端蓝色主题
    Theme::applyExpertTheme(this);

    // 实时通讯
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT, "N/A", g_expertUsername);

    // Tab 切换时激活通讯
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

    // 右上角“更改账号”按钮（不改 .ui，挂到 TabBar 角落）
    {
        QPushButton* btnSwitch = new QPushButton(QStringLiteral("更改账号"), ui->tabWidget);
        btnSwitch->setToolTip(QStringLiteral("返回登录页以更换账号（快捷键：Ctrl+L）"));
        btnSwitch->setCursor(Qt::PointingHandCursor);
        btnSwitch->setObjectName("btnSwitchAccount");
        btnSwitch->setStyleSheet(
            "QPushButton#btnSwitchAccount {"
            "  background: rgba(59,130,246,0.22);"
            "  border: 1px solid #3b82f6; color: #e6f0ff; border-radius: 6px; padding: 4px 10px;"
            "}"
            "QPushButton#btnSwitchAccount:hover { background: rgba(147,197,253,0.35); }"
        );
        ui->tabWidget->setCornerWidget(btnSwitch, Qt::TopRightCorner);
        connect(btnSwitch, &QPushButton::clicked, this, [this]{ logoutToLogin(); });
    }
    // 兼容旧版 Qt：先创建，再连 activated
    {
        auto* sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
        connect(sc, &QShortcut::activated, this, &ClientExpert::logoutToLogin);
    }

    // 角色化 UI 与表格装饰
    applyRoleUi();
    decorateOrdersTable();

    // 状态筛选（保持）
    ui->comboBoxStatus->clear();
    ui->comboBoxStatus->addItem("全部");
    ui->comboBoxStatus->addItem("待处理");
    ui->comboBoxStatus->addItem("已接受");
    ui->comboBoxStatus->addItem("已拒绝");

    // 双击查看详情
    connect(ui->tableOrders, &QTableWidget::cellDoubleClicked, this, &ClientExpert::onOrderDoubleClicked);

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

    this->setStyleSheet(this->styleSheet() +
        " QHeaderView::section { background: rgba(59,130,246,0.18); }"
        " QTableWidget { border: 1px solid rgba(59,130,246,0.55); }"
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
    // 导航始终可切换；未接单时切到需要权限的页会弹提示（见 on_tabChanged）
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
        OrderInfo info{
            o.value("id").toInt(),
            o.value("title").toString(),
            o.value("desc").toString(),
            o.value("status").toString(),
            QString(), QString()
        };
        // 兼容不同键名
        info.publisher = o.value("publisher").toString();
        if (info.publisher.isEmpty()) info.publisher = o.value("factory_user").toString();
        info.accepter = o.value("accepter").toString();
        if (info.accepter.isEmpty()) info.accepter = o.value("expert_user").toString();
        orders.append(info);
    }
    auto* tbl = ui->tableOrders;
    tbl->clear();
    tbl->setColumnCount(6);
    tbl->setRowCount(orders.size());
    QStringList headers{"工单号", "标题", "描述", "发布者", "接受者", "状态"};
    tbl->setHorizontalHeaderLabels(headers);
    for (int i = 0; i < orders.size(); ++i) {
        const auto& od = orders[i];
        const QColor fg = statusColor(od.status);
        const QColor bg = QColor(fg.red(), fg.green(), fg.blue(), 26);

        auto put = [&](int col, const QString& text){
            auto* it = new QTableWidgetItem(text);
            it->setForeground(QBrush(fg));
            it->setBackground(QBrush(bg));
            tbl->setItem(i, col, it);
        };
        put(0, QString::number(od.id));
        put(1, od.title);
        put(2, od.desc);
        put(3, od.publisher);
        put(4, od.accepter);
        put(5, od.status);
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
    // 接单后为通讯设置房间名
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT,
                                   QString("order-%1").arg(id), g_expertUsername);
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
    // 当专家接受时，把接受者写入，便于服务端与另一端展示
    if (status == QStringLiteral("已接受")) {
        req["expert_user"] = g_expertUsername;
    }
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
    // 未接受工单时，阻止进入需要权限的 Tab，并美化提示
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
    layout->addWidget(mk("发布者：", od.publisher));
    layout->addWidget(mk("接受者：", od.accepter));
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
    dlg.resize(560, 360);
    dlg.exec();
}

void ClientExpert::logoutToLogin()
{
    g_expertUsername.clear();
    // 打开登录窗口并关闭当前窗口
    Login* lg = new Login();
    lg->show();
    this->close();
}
