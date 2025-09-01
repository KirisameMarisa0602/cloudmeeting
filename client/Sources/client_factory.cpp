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

ClientFactory::ClientFactory(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClientFactory)
{
    ui->setupUi(this);

    // 工厂端绿/浅绿主题（导航选中“实心绿色块”）
    Theme::applyFactoryTheme(this);

    // 实时通讯容器
    commWidget_ = new CommWidget(this);
    ui->verticalLayoutTabRealtime->addWidget(commWidget_);
    commWidget_->setConnectionInfo(QString::fromLatin1(SERVER_HOST), SERVER_PORT, "N/A", UserSession::factoryUsername);

    // Tab 角落：左侧显示当前用户名称，右侧“更改账号”
    labUserNameCorner_ = new QLabel(QStringLiteral("用户：") + UserSession::factoryUsername, ui->tabWidget);
    labUserNameCorner_->setStyleSheet("padding:4px 10px; color:#e8ffee;");
    ui->tabWidget->setCornerWidget(labUserNameCorner_, Qt::TopLeftCorner);

    QPushButton* btnSwitch = new QPushButton(QStringLiteral("更改账号"), ui->tabWidget);
    btnSwitch->setToolTip(QStringLiteral("返回登录页以更换账号（快捷键：Ctrl+L）"));
    btnSwitch->setCursor(Qt::PointingHandCursor);
    btnSwitch->setObjectName("btnSwitchAccount");
    btnSwitch->setStyleSheet(
        "QPushButton#btnSwitchAccount { background: rgba(34,197,94,0.22); border: 1px solid #22c55e; color: #e8ffee; border-radius: 6px; padding: 4px 10px; }"
        "QPushButton#btnSwitchAccount:hover { background: rgba(134,239,172,0.35); }"
    );
    ui->tabWidget->setCornerWidget(btnSwitch, Qt::TopRightCorner);
    connect(btnSwitch, &QPushButton::clicked, this, [this]{ logoutToLogin(); });

    auto* sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(sc, &QShortcut::activated, this, &ClientFactory::logoutToLogin);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ClientFactory::on_tabChanged);
    connect(ui->btnSearchOrder, &QPushButton::clicked, this, &ClientFactory::onSearchOrder);
    connect(ui->btnRefreshOrderStatus, &QPushButton::clicked, this, &ClientFactory::refreshOrders);
    connect(ui->btnDeleteOrder, &QPushButton::clicked, this, &ClientFactory::on_btnDeleteOrder_clicked);

    applyRoleUi();
    decorateOrdersTable();

    connect(ui->tableOrders, &QTableWidget::cellDoubleClicked, this, &ClientFactory::onOrderDoubleClicked);

    refreshOrders();
    updateTabEnabled();
}

ClientFactory::~ClientFactory() { delete ui; }

void ClientFactory::applyRoleUi()
{
    setWindowTitle(QStringLiteral("工厂端 | 智能协同云会议"));
    if (ui->tabWidget && ui->tabWidget->count() > 0)
        ui->tabWidget->setTabText(0, QStringLiteral("工厂端 • 工单管理"));

    if (ui->lineEditKeyword)
        ui->lineEditKeyword->setPlaceholderText(QStringLiteral("按工单号/标题关键词搜索…"));

    if (ui->comboBoxStatus && ui->comboBoxStatus->count() == 0)
        ui->comboBoxStatus->addItems(QStringList() << "全部" << "待处理" << "已接受" << "已拒绝");

    this->setStyleSheet(this->styleSheet() +
        " QHeaderView::section { background: rgba(34,197,94,0.18); }"
        " QTableWidget { border: 1px solid rgba(34,197,94,0.55); }"
    );
}

void ClientFactory::decorateOrdersTable()
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

void ClientFactory::refreshOrders()
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) { QMessageBox::warning(this, "提示", "无法连接服务器"); return; }

    QJsonObject req{{"action", "get_orders"}};
    req["role"] = "factory";
    req["username"] = UserSession::factoryUsername;
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
        const QColor bg = QColor(fg.red(), fg.green(), fg.blue(), 24);
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

void ClientFactory::on_btnNewOrder_clicked()
{
    NewOrderDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString title = dlg.editTitle->text().trimmed();
        QString desc  = dlg.editDesc->toPlainText().trimmed();
        if (title.isEmpty()) { QMessageBox::warning(this, "提示", "工单标题不能为空"); return; }
        sendCreateOrder(title, desc);
        QTimer::singleShot(150, this, [this]{ refreshOrders(); });
    }
}

void ClientFactory::sendCreateOrder(const QString& title, const QString& desc)
{
    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) { QMessageBox::warning(this, "提示", "无法连接服务器"); return; }

    QJsonObject req{
        {"action", "new_order"},
        {"title", title},
        {"desc", desc},
        {"factory_account", UserSession::factoryAccount}, // 账户
        {"publisher",       UserSession::factoryUsername} // 用户名称
    };
    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForReadyRead(2000);
}

void ClientFactory::on_btnDeleteOrder_clicked()
{
    if (deletingOrder) return;
    deletingOrder = true;

    int row = ui->tableOrders->currentRow();
    if (row < 0 || row >= orders.size()) { QMessageBox::warning(this, "提示", "请选择要销毁的工单"); deletingOrder = false; return; }
    int id = orders[row].id;
    if (QMessageBox::question(this, "确认", "确定要销毁该工单？") != QMessageBox::Yes) { deletingOrder = false; return; }

    QTcpSocket sock;
    sock.connectToHost(SERVER_HOST, SERVER_PORT);
    if (!sock.waitForConnected(2000)) { QMessageBox::warning(this, "提示", "无法连接服务器"); deletingOrder = false; return; }

    QJsonObject req{ {"action","delete_order"}, {"id", id} };
    sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    sock.waitForReadyRead(2000);

    QTimer::singleShot(150, this, [this]{ refreshOrders(); deletingOrder = false; });
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

void ClientFactory::onSearchOrder() { refreshOrders(); }

void ClientFactory::onOrderDoubleClicked(int row, int)
{
    if (row < 0 || row >= orders.size()) return;
    const auto& od = orders[row];
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("工单详情（工厂端）"));
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    auto mk = [](const QString& k, const QString& v){ QWidget* w=new QWidget; QHBoxLayout* h=new QHBoxLayout(w); h->setContentsMargins(0,0,0,0); QLabel* lk=new QLabel("<b>"+k+"</b>"); QLabel* lv=new QLabel(v); lv->setTextInteractionFlags(Qt::TextSelectableByMouse); h->addWidget(lk); h->addWidget(lv,1); return w; };
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

void ClientFactory::logoutToLogin()
{
    UserSession::factoryAccount.clear();
    UserSession::factoryUsername.clear();
    Login* lg = new Login(); lg->show(); this->close();
}
