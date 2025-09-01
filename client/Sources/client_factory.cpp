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
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

extern QString g_factoryUsername;

static QColor statusColor(const QString& s) {
    if (s == QStringLiteral("已接受")) return QColor(16, 163, 74);   // green
    if (s == QStringLiteral("已拒绝")) return QColor(220, 38, 38);   // red
    return QColor(234, 179, 8);                                      // amber
}

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

    // 角色化 UI 与表格装饰
    applyRoleUi();
    decorateOrdersTable();

    // 双击查看详情
    connect(ui->tableOrders, &QTableWidget::cellDoubleClicked,
            this, &ClientFactory::onOrderDoubleClicked);

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

    // Tab 文案更贴近角色语义（不更改索引结构）
    if (ui->tabWidget && ui->tabWidget->count() > 0) {
        ui->tabWidget->setTabText(0, QStringLiteral("工厂端 • 工单管理"));
    }

    // 搜索提示
    if (ui->lineEditKeyword) {
        ui->lineEditKeyword->setPlaceholderText(QStringLiteral("按工单号/标题关键词搜索…"));
    }

    // 若 UI 里状态下拉未初始化，这里做兜底（避免覆盖设计器里已有设置）
    if (ui->comboBoxStatus && ui->comboBoxStatus->count() == 0) {
        ui->comboBoxStatus->addItem("全部");
        ui->comboBoxStatus->addItem("待处理");
        ui->comboBoxStatus->addItem("已接受");
        ui->comboBoxStatus->addItem("已拒绝");
    }

    // 按钮提示
    if (ui->btnSearchOrder) ui->btnSearchOrder->setToolTip(QStringLiteral("按关键词与状态筛选"));
    if (ui->btnRefreshOrderStatus) ui->btnRefreshOrderStatus->setToolTip(QStringLiteral("刷新工单列表"));
    if (ui->btnDeleteOrder) ui->btnDeleteOrder->setToolTip(QStringLiteral("销毁所选工单"));
    // btnNewOrder 点击信号一般由 UI 自动连接（on_btnNewOrder_clicked），这里不重复连接
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

    // 为深色主题优化的表头样式（不会影响逻辑）
    this->setStyleSheet(this->styleSheet() +
        " QHeaderView::section { background: rgba(255,255,255,0.06); color: #e5e7eb; border: 0; padding: 6px 8px; }"
        " QTableWidget { gridline-color: rgba(229,231,235,0.1); }"
    );
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
        auto* itemId = new QTableWidgetItem(QString::number(od.id));
        auto* itemTitle = new QTableWidgetItem(od.title);
        auto* itemDesc = new QTableWidgetItem(od.desc);
        auto* itemStatus = new QTableWidgetItem(od.status);

        // 状态色彩（前景色），并为整行赋予柔和色彩，便于一眼区分
        const QColor fg = statusColor(od.status);
        itemId->setForeground(QBrush(fg));
        itemTitle->setForeground(QBrush(fg));
        itemDesc->setForeground(QBrush(fg));
        itemStatus->setForeground(QBrush(fg));

        tbl->setItem(i, 0, itemId);
        tbl->setItem(i, 1, itemTitle);
        tbl->setItem(i, 2, itemDesc);
        tbl->setItem(i, 3, itemStatus);
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
