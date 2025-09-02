#include "client_expert.h"
#include "ui_client_expert.h"
#include "theme.h"
#include "user_session.h"
#include "login.h"
#include "net_util.h"
#include "shared_types.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QShortcut>
#include <QMenu>
#include <QAction>

static QColor statusColor(const QString& s) {
    if (s == QStringLiteral("待处理")) return QColor("#E6A23C");
    if (s == QStringLiteral("已接受")) return QColor("#67C23A");
    if (s == QStringLiteral("已拒绝")) return QColor("#F56C6C");
    return QColor("#ffffff");
}

static OrderInfo orderFromJson(const QJsonObject& o)
{
    OrderInfo r;
    r.id = o.value("id").toInt();
    r.title = o.value("title").toString();
    r.desc = o.value("desc").toString();
    r.status = o.value("status").toString();
    r.publisher = o.value("publisher").toString();
    r.accepter = o.value("accepter").toString();
    return r;
}

ClientExpert::ClientExpert(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientExpert)
{
    ui->setupUi(this);
    Theme::applyExpertTheme(this);
    applyRoleUi();

    // 通讯区域（右侧在线聊天）
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);
    commWidget_->setConnectionInfo(QString::fromLatin1(serverHost()), serverPort(), "Room1", UserSession::expertUsername);

    // 顶部角落标签与切账号按钮
    labUserNameCorner_ = new QLabel(QStringLiteral("用户：") + UserSession::expertUsername, ui->tabWidget);
    labUserNameCorner_->setStyleSheet("padding:4px 10px; color:#e5edff;");
    ui->tabWidget->setCornerWidget(labUserNameCorner_, Qt::TopLeftCorner);

    QPushButton* btnSwitch = new QPushButton(QStringLiteral("更改账号"), ui->tabWidget);
    btnSwitch->setToolTip(QStringLiteral("返回登录页以更换账号（快捷键：Ctrl+L）"));
    btnSwitch->setCursor(Qt::PointingHandCursor);
    btnSwitch->setObjectName("btnSwitchAccount");
    ui->tabWidget->setCornerWidget(btnSwitch, Qt::TopRightCorner);
    connect(btnSwitch, &QPushButton::clicked, this, [this]{ logoutToLogin(); });

    auto* sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(sc, &QShortcut::activated, this, &ClientExpert::logoutToLogin);

    // 表格装饰与信号
    decorateOrdersTable();
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ClientExpert::on_tabChanged);
    connect(ui->btnAccept, &QPushButton::clicked, this, &ClientExpert::on_btnAccept_clicked);
    connect(ui->btnReject, &QPushButton::clicked, this, &ClientExpert::on_btnReject_clicked);
    connect(ui->btnSearchOrder, &QPushButton::clicked, this, &ClientExpert::onSearchOrder);
    if (auto btn = this->findChild<QPushButton*>("btnRefreshOrderStatus"))
        connect(btn, &QPushButton::clicked, this, &ClientExpert::onSearchOrder);

    refreshOrders();
    updateTabEnabled();
}

ClientExpert::~ClientExpert() { delete ui; }

void ClientExpert::applyRoleUi() {}

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

    // 新增：右键菜单支持状态切换（回滚/二次流转）
    t->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(t, &QTableWidget::customContextMenuRequested,
            this, &ClientExpert::onOrdersTableContextMenuRequested);
}

void ClientExpert::setJoinedOrder(bool joined) { joinedOrder = joined; updateTabEnabled(); }

void ClientExpert::updateTabEnabled()
{
    bool hasSelection = ui->tableOrders->currentRow() >= 0;
    QString status;
    if (hasSelection) status = ui->tableOrders->item(ui->tableOrders->currentRow(), 3)->text();

    // 允许二次流转：当选中时启用对应按钮
    // - 接受：除非当前已是“已接受”
    // - 拒绝：除非当前已是“已拒绝”
    const bool canAccept = hasSelection && status != QStringLiteral("已接受");
    const bool canReject = hasSelection && status != QStringLiteral("已拒绝");

    ui->btnAccept->setEnabled(canAccept);
    ui->btnReject->setEnabled(canReject);
}

void ClientExpert::refreshOrders()
{
    QJsonObject rep;
    QString err;
    if (!sendRequest(QJsonObject{
        {"action","get_orders"},
        {"status", ui->comboBoxStatus ? ui->comboBoxStatus->currentText() : QString()},
        {"keyword", ui->lineEditKeyword ? ui->lineEditKeyword->text().trimmed() : QString()}
    }, rep, &err)) {
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

    QString selStatus; if (ui->comboBoxStatus) selStatus = ui->comboBoxStatus->currentText();

    auto* t = ui->tableOrders;
    t->setRowCount(0);
    for (const auto& od : orders) {
        if (!selStatus.isEmpty() && selStatus != "全部" && od.status != selStatus) continue;
        int r = t->rowCount(); t->insertRow(r);
        auto idItem = new QTableWidgetItem(QString::number(od.id)); idItem->setData(Qt::UserRole, od.id);
        auto titleItem = new QTableWidgetItem(od.title);
        auto descItem = new QTableWidgetItem(od.desc);
        auto statusItem = new QTableWidgetItem(od.status); statusItem->setForeground(statusColor(od.status));
        t->setItem(r,0,idItem); t->setItem(r,1,titleItem); t->setItem(r,2,descItem);
        t->setItem(r,3,statusItem); t->setItem(r,4,new QTableWidgetItem(od.publisher));
        t->setItem(r,5,new QTableWidgetItem(od.accepter.isEmpty() ? "-" : od.accepter));
    }

    // 是否存在“我已接受”的工单，用于后续权限控制
    setJoinedOrder(hasMyAcceptedOrder());
}

void ClientExpert::sendUpdateOrder(int orderId, const QString& status)
{
    // 放开前端限制：允许对任意状态进行更新（权限在后端校验）
    QJsonObject rep;
    QString err;
    QJsonObject req{{"action","update_order"},{"id",orderId},{"status",status}};
    // 重要：所有状态更新都携带当前专家用户名，便于服务端做权限控制
    req["accepter"] = UserSession::expertUsername;

    if (!sendRequest(req, rep, &err)) { QMessageBox::warning(this, "更新工单失败", err); return; }
    if (!rep.value("ok").toBool()) { QMessageBox::warning(this, "更新工单失败", rep.value("msg").toString("未知错误")); return; }
    refreshOrders();
}

void ClientExpert::on_btnAccept_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0) { QMessageBox::information(this, "提示", "请选择一条工单"); return; }
    int id = ui->tableOrders->item(row,0)->data(Qt::UserRole).toInt();
    sendUpdateOrder(id, QStringLiteral("已接受"));
}

void ClientExpert::on_btnReject_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0) { QMessageBox::information(this, "提示", "请选择一条工单"); return; }
    int id = ui->tableOrders->item(row,0)->data(Qt::UserRole).toInt();
    sendUpdateOrder(id, QStringLiteral("已拒绝"));
}

void ClientExpert::on_tabChanged(int idx)
{
    // 假设第 0 页是“工单设置”，其它页需要“我已接受的工单”上下文
    if (idx != 0 && !joinedOrder) {
        QMessageBox::information(this, "提示", "当前没有待处理工单或你尚未加入任何工单");
        ui->tabWidget->setCurrentIndex(0);
        return;
    }
    updateTabEnabled();
}

void ClientExpert::onSearchOrder() { refreshOrders(); }

void ClientExpert::showOrderDetailsDialog(const OrderInfo& od)
{
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
}

void ClientExpert::onOrderDoubleClicked(int row, int)
{
    if (row < 0) return;
    int id = ui->tableOrders->item(row,0)->data(Qt::UserRole).toInt();
    for (const auto& od : orders) if (od.id == id) { showOrderDetailsDialog(od); break; }
}

bool ClientExpert::hasMyAcceptedOrder() const
{
    for (const auto& od : orders) {
        if (od.status == QStringLiteral("已接受") && od.accepter == UserSession::expertUsername)
            return true;
    }
    return false;
}

void ClientExpert::logoutToLogin()
{
    this->hide();
    auto* login = new Login();
    login->show();
}

void ClientExpert::onOrdersTableContextMenuRequested(const QPoint& pos)
{
    auto* t = ui->tableOrders;
    int row = t->rowAt(pos.y());
    if (row < 0) return;

    const int id = t->item(row,0)->data(Qt::UserRole).toInt();
    const QString curStatus = t->item(row,3)->text();

    QMenu menu(this);
    QAction* actPending = menu.addAction(QStringLiteral("设为：待处理"));
    QAction* actAccept  = menu.addAction(QStringLiteral("设为：已接受"));
    QAction* actReject  = menu.addAction(QStringLiteral("设为：已拒绝"));

    actPending->setEnabled(curStatus != QStringLiteral("待处理"));
    actAccept->setEnabled(curStatus != QStringLiteral("已接受"));
    actReject->setEnabled(curStatus != QStringLiteral("已拒绝"));

    QAction* chosen = menu.exec(t->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actPending) {
        sendUpdateOrder(id, QStringLiteral("待处理"));
    } else if (chosen == actAccept) {
        sendUpdateOrder(id, QStringLiteral("已接受"));
    } else if (chosen == actReject) {
        sendUpdateOrder(id, QStringLiteral("已拒绝"));
    }
}
