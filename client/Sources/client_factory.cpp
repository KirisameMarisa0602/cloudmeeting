#include "client_factory.h"
#include "ui_client_factory.h"
#include "comm/commwidget.h"
#include "theme.h"
#include "login.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QInputDialog>
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

extern QString g_factoryUsername;

static QColor statusColor(const QString& s) {
    if (s == QStringLiteral("已接受")) return QColor(34, 197, 94);   // 绿
    if (s == QStringLiteral("已拒绝")) return QColor(220, 38, 38);   // 红
    return QColor(234, 179, 8);                                      // 橙
}

class NewOrderDialog : public QDialog {
public:
    QLineEdit* editTitle;
    QTextEdit* editDesc;
    NewOrderDialog(QWidget* parent = nullptr)
        : QDialog(parent)
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

    // 应用工厂端绿色主题
    Theme::applyFactoryTheme(this);

    // 实时通讯模块
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT, "N/A", g_factoryUsername);

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

    // 右上角“更改账号”按钮（挂到 TabBar 角落）
    {
        QPushButton* btnSwitch = new QPushButton(QStringLiteral("更改账号"), ui->tabWidget);
        btnSwitch->setToolTip(QStringLiteral("返回登录页以更换账号（快捷键：Ctrl+L）"));
        btnSwitch->setCursor(Qt::PointingHandCursor);
        btnSwitch->setObjectName("btnSwitchAccount");
        btnSwitch->setStyleSheet(
            "QPushButton#btnSwitchAccount {"
            "  background: rgba(34,197,94,0.22);"
            "  border: 1px solid #22c55e; color: #e8ffee; border-radius: 6px; padding: 4px 10px;"
            "}"
            "QPushButton#btnSwitchAccount:hover { background: rgba(134,239,172,0.35); }"
        );
        ui->tabWidget->setCornerWidget(btnSwitch, Qt::TopRightCorner);
        connect(btnSwitch, &QPushButton::clicked, this, [this]{ logoutToLogin(); });
    }
    // 快捷键：Ctrl+L 返回登录
    {
        auto* sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
        connect(sc, &QShortcut::activated, this, &ClientFactory::logoutToLogin);
    }

    // 角色化 UI 与表格装饰
    applyRoleUi();
    decorateOrdersTable();

    // 双击查看详情
    connect(ui->tableOrders, &QTableWidget::cellDoubleClicked, this, &ClientFactory::onOrderDoubleClicked);

    refreshOrders();
    updateTabEnabled();
}

ClientFactory::~ClientFactory()
{
    delete ui;
}

void ClientFactory::applyRoleUi()
{
    setWindowTitle(QStringLiteral("工厂端 | 智能协同云会议"));

    if (ui->tabWidget && ui->tabWidget->count() > 0) {
        ui->tabWidget->setTabText(0, QStringLiteral("工厂端 • 工单管理"));
    }

    if (ui->lineEditKeyword) {
        ui->lineEditKeyword->setPlaceholderText(QStringLiteral("按工单号/标题关键词搜索…"));
    }

    if (ui->comboBoxStatus && ui->comboBoxStatus->count() == 0) {
        ui->comboBoxStatus->addItem("全部");
        ui->comboBoxStatus->addItem("待处理");
        ui->comboBoxStatus->addItem("已接受");
        ui->comboBoxStatus->addItem("已拒绝");
    }

    if (ui->btnSearchOrder) ui->btnSearchOrder->setToolTip(QStringLiteral("按关键词与状态筛选"));
    if (ui->btnRefreshOrderStatus) ui->btnRefreshOrderStatus->setToolTip(QStringLiteral("刷新工单列表"));
    if (ui->btnDeleteOrder) ui->btnDeleteOrder->setToolTip(QStringLiteral("销毁所选工单"));

    this->setStyleSheet(this->styleSheet() +
        " QHeaderView::section { background: rgba(34,197,94,0.18); }"
        " QTableWidget { border: 1px solid rgba(34,197,94,0.55); }"
    );
}

void ClientFactory::decorateOrdersTable()
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
    QStringList headers{"工单号", "标题", "描述", "发布者", "接受者", "状态"};
    tbl->setHorizontalHeaderLabels(headers);
    for (int i = 0; i < orders.size(); ++i) {
        const auto& od = orders[i];
        const QColor fg = statusColor(od.status);
        const QColor bg = QColor(fg.red(), fg.green(), fg.blue(), 24);

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
    // 已有字段 factory_user 作为发布者
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

void ClientFactory::onOrderDoubleClicked(int row, int /*column*/)
{
    if (row < 0 || row >= orders.size()) return;

    const OrderInfo& od = orders[row];
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("工单详情（工厂端）"));
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

void ClientFactory::logoutToLogin()
{
    g_factoryUsername.clear();
    Login* lg = new Login();
    lg->show();
    this->close();
}
