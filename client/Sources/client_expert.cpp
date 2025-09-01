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

ClientExpert::ClientExpert(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientExpert)
{
    ui->setupUi(this);
    Theme::applyExpertTheme(this);

    // 实时通讯容器
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT, "N/A", UserSession::expertUsername);

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

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ClientExpert::on_tabChanged);
    connect(ui->btnAccept, &QPushButton::clicked, this, &ClientExpert::on_btnAccept_clicked);
    connect(ui->btnReject, &QPushButton::clicked, this, &ClientExpert::on_btnReject_clicked);
    connect(ui->btnRefreshOrderStatus, &QPushButton::clicked, this, &ClientExpert::refreshOrders);
    connect(ui->btnSearchOrder, &QPushButton::clicked, this, &ClientExpert::onSearchOrder);

    applyRoleUi();
    decorateOrdersTable();

    ui->comboBoxStatus->clear();
    ui->comboBoxStatus->addItems(QStringList() << "全部" << "待处理" << "已接受" << "已拒绝");

    connect(ui->tableOrders, &QTableWidget::cellDoubleClicked, this, &ClientExpert::onOrderDoubleClicked);

    refreshOrders();
    updateTabEnabled();
}

ClientExpert::~ClientExpert() { delete ui; }

void ClientExpert::applyRoleUi()
{
    setWindowTitle(QStringLiteral("专家端 | 智能协同云会议"));
    if (ui->tabWidget && ui->tabWidget->count() > 0)
        ui->tabWidget->setTabText(0, QStringLiteral("专家端 • 工单中心"));

    if (ui->lineEditKeyword)
        ui->lineEditKeyword->setPlaceholderText(QStringLiteral("按工单号/标题关键词搜索…"));

    this->setStyleSheet(this->styleSheet() +
        " QHeaderView::section { background: rgba(59,130,246,0.18); }"
        " QTableWidget { border: 1px solid rgba(59,130,246,0.55); }"
    );
}

void ClientExpert::decorateOrdersTable()
{
    auto* tbl = ui->tableOrders;
    tbl->setAlternatingRowColors(true);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setSelectionMode(QAbstractItemView::SingleSelection);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->verticalHeader()->setVisible(false);
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setShowGrid(true);
}

void ClientExpert::setJoinedOrder(bool joined) { joinedOrder = joined; updateTabEnabled(); }

void ClientExpert::updateTabEnabled()
{
    ui->tabWidget->setTabEnabled(1, true);
    ui->tabWidget->setTabEnabled(3, true);
}

void ClientExpert::refreshOrders()
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) { QMessageBox::warning(this, "提示", "无法连接服务器"); return; }

    QJsonObject req{{"action", "get_orders"}};
    req["role"] = "expert";
    req["username"] = UserSession::expertUsername;
    QString keyword = ui->lineEditKeyword->text().trimmed();
    if (!keyword.isEmpty()) req["keyword"] = keyword;
    QString status = ui->comboBoxStatus->currentText();
    if (status != "全部") req["status"] = status;

    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForReadyRead(2000);
    QByteArray resp = sock.readAll();
    int nl = resp.indexOf('\n'); if (nl >= 0) resp = resp.left(nl);
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject() || !doc.object().value("ok").toBool()) { QMessageBox::warning(this, "提示", "服务器响应异常"); return; }

    orders.clear();
    for (const QJsonValue& v : doc.object().value("orders").toArray()) {
        QJsonObject o = v.toObject();
        OrderInfo info{
            o.value("id").toInt(),
            o.value("title").toString(),
            o.value("desc").toString(),
            o.value("status").toString(),
            QString(), QString()
        };
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
    tbl->setHorizontalHeaderLabels(QStringList() << "工单号" << "标题" << "描述" << "发布者" << "接受者" << "状态");

    for (int i = 0; i < orders.size(); ++i) {
        const auto& od = orders[i];
        const QColor fg = statusColor(od.status);
        const QColor bg = QColor(fg.red(), fg.green(), fg.blue(), 26);
        auto put = [&](int c, const QString& t){ auto* it=new QTableWidgetItem(t); it->setForeground(QBrush(fg)); it->setBackground(QBrush(bg)); tbl->setItem(i,c,it); };
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
    if (row < 0 || row >= orders.size()) { QMessageBox::warning(this, "提示", "请选择一个工单"); return; }
    int id = orders[row].id;

    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) { QMessageBox::warning(this, "提示", "无法连接服务器"); return; }

    QJsonObject req{{"action","update_order"}, {"id", id}, {"status","已接受"}};
    req["expert_account"] = UserSession::expertAccount;  // 账户
    req["accepter"]       = UserSession::expertUsername; // 用户名称
    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForReadyRead(2000);

    setJoinedOrder(true);
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT, QString("order-%1").arg(id), UserSession::expertUsername);
    QTimer::singleShot(150, this, [this]{ refreshOrders(); });
}

void ClientExpert::on_btnReject_clicked()
{
    int row = ui->tableOrders->currentRow();
    if (row < 0 || row >= orders.size()) { QMessageBox::warning(this, "提示", "请选择一个工单"); return; }
    int id = orders[row].id;

    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) { QMessageBox::warning(this, "提示", "无法连接服务器"); return; }

    QJsonObject req{{"action","update_order"}, {"id", id}, {"status","已拒绝"}};
    req["expert_account"] = UserSession::expertAccount;
    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForReadyRead(2000);

    setJoinedOrder(false);
    QTimer::singleShot(150, this, [this]{ refreshOrders(); });
}

void ClientExpert::on_tabChanged(int idx)
{
    if ((idx == 1 || idx == 3) && !joinedOrder) { QMessageBox::information(this, "提示", "暂无待处理工单"); ui->tabWidget->setCurrentIndex(0); return; }
    if (idx == 0) refreshOrders();
}

void ClientExpert::onSearchOrder() { refreshOrders(); }

void ClientExpert::onOrderDoubleClicked(int row, int)
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
        QWidget* w = new QWidget; QHBoxLayout* h = new QHBoxLayout(w); h->setContentsMargins(0,0,0,0);
        QLabel* lk = new QLabel("<b>" + k + "</b>"); QLabel* lv = new QLabel(v); lv->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(lk); h->addWidget(lv, 1); return w;
    };
    layout->addWidget(mk("工单号：", QString::number(od.id)));
    layout->addWidget(mk("标题：", od.title));
    layout->addWidget(mk("发布者：", od.publisher));
    layout->addWidget(mk("接受者：", od.accepter));
    layout->addWidget(mk("状态：", od.status));
    QLabel* desc = new QLabel("<b>描述：</b>"); QLabel* dval = new QLabel(od.desc); dval->setWordWrap(true); dval->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(desc); layout->addWidget(dval);
    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    layout->addWidget(box);
    dlg.resize(560, 360);
    dlg.exec();
}

void ClientExpert::logoutToLogin()
{
    UserSession::expertAccount.clear();
    UserSession::expertUsername.clear();
    Login* lg = new Login(); lg->show(); this->close();
}
