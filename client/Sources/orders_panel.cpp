#include "orders_panel.h"
#include "clientconn.h"
#include "protocol.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QSplitter>
#include <QHeaderView>
#include <QDateTime>
#include <QFormLayout>

OrdersPanel::OrdersPanel(QWidget *parent)
    : QWidget(parent)
    , conn_(nullptr)
{
    setupUI();
}

OrdersPanel::~OrdersPanel()
{
}

void OrdersPanel::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setSpacing(10);
    mainLayout_->setContentsMargins(10, 10, 10, 10);

    // Title and refresh button
    auto* headerLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel("Work Orders", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #2c3e50;");
    
    refreshBtn_ = new QPushButton("Refresh", this);
    refreshBtn_->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; border: none; padding: 5px 15px; border-radius: 3px; } QPushButton:hover { background-color: #7f8c8d; }");
    connect(refreshBtn_, &QPushButton::clicked, this, &OrdersPanel::onRefreshClicked);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(refreshBtn_);
    mainLayout_->addLayout(headerLayout);

    // Create stacked widget for different views
    viewStack_ = new QStackedWidget(this);
    
    // Setup factory and expert views
    setupFactoryView();
    setupExpertView();
    
    mainLayout_->addWidget(viewStack_);

    // Order details panel (shared)
    detailsGroup_ = new QGroupBox("Order Details", this);
    detailsGroup_->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #bdc3c7; border-radius: 5px; margin: 5px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }");
    auto* detailsLayout = new QVBoxLayout(detailsGroup_);

    // Order info labels
    auto* infoLayout = new QFormLayout();
    orderIdLabel_ = new QLabel("—", this);
    orderTitleLabel_ = new QLabel("—", this);
    orderDescLabel_ = new QLabel("—", this);
    orderDescLabel_->setWordWrap(true);
    orderStatusLabel_ = new QLabel("—", this);
    orderCreatedByLabel_ = new QLabel("—", this);
    orderAssignedToLabel_ = new QLabel("—", this);
    orderCreatedAtLabel_ = new QLabel("—", this);
    orderUpdatedAtLabel_ = new QLabel("—", this);

    infoLayout->addRow("ID:", orderIdLabel_);
    infoLayout->addRow("Title:", orderTitleLabel_);
    infoLayout->addRow("Description:", orderDescLabel_);
    infoLayout->addRow("Status:", orderStatusLabel_);
    infoLayout->addRow("Created By:", orderCreatedByLabel_);
    infoLayout->addRow("Assigned To:", orderAssignedToLabel_);
    infoLayout->addRow("Created:", orderCreatedAtLabel_);
    infoLayout->addRow("Updated:", orderUpdatedAtLabel_);
    
    detailsLayout->addLayout(infoLayout);

    // Action buttons
    auto* actionLayout = new QHBoxLayout();
    
    acceptBtn_ = new QPushButton("Accept", this);
    acceptBtn_->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border: none; padding: 8px 15px; border-radius: 3px; } QPushButton:hover { background-color: #229954; } QPushButton:disabled { background-color: #bdc3c7; }");
    connect(acceptBtn_, &QPushButton::clicked, this, &OrdersPanel::onAcceptOrderClicked);
    
    rejectBtn_ = new QPushButton("Reject", this);
    rejectBtn_->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border: none; padding: 8px 15px; border-radius: 3px; } QPushButton:hover { background-color: #c0392b; } QPushButton:disabled { background-color: #bdc3c7; }");
    connect(rejectBtn_, &QPushButton::clicked, this, &OrdersPanel::onRejectOrderClicked);
    
    openRoomBtn_ = new QPushButton("Open Room", this);
    openRoomBtn_->setStyleSheet("QPushButton { background-color: #3498db; color: white; border: none; padding: 8px 15px; border-radius: 3px; } QPushButton:hover { background-color: #2980b9; } QPushButton:disabled { background-color: #bdc3c7; }");
    connect(openRoomBtn_, &QPushButton::clicked, this, &OrdersPanel::onOpenRoomClicked);
    
    closeOrderBtn_ = new QPushButton("Close", this);
    closeOrderBtn_->setStyleSheet("QPushButton { background-color: #f39c12; color: white; border: none; padding: 8px 15px; border-radius: 3px; } QPushButton:hover { background-color: #e67e22; } QPushButton:disabled { background-color: #bdc3c7; }");
    connect(closeOrderBtn_, &QPushButton::clicked, this, &OrdersPanel::onCloseOrderClicked);
    
    cancelOrderBtn_ = new QPushButton("Cancel", this);
    cancelOrderBtn_->setStyleSheet("QPushButton { background-color: #95a5a6; color: white; border: none; padding: 8px 15px; border-radius: 3px; } QPushButton:hover { background-color: #7f8c8d; } QPushButton:disabled { background-color: #bdc3c7; }");
    connect(cancelOrderBtn_, &QPushButton::clicked, this, &OrdersPanel::onCancelOrderClicked);

    actionLayout->addWidget(acceptBtn_);
    actionLayout->addWidget(rejectBtn_);
    actionLayout->addStretch();
    actionLayout->addWidget(openRoomBtn_);
    actionLayout->addWidget(closeOrderBtn_);
    actionLayout->addWidget(cancelOrderBtn_);
    
    detailsLayout->addLayout(actionLayout);
    mainLayout_->addWidget(detailsGroup_);

    // Initially hide details
    detailsGroup_->setVisible(false);
}

void OrdersPanel::setupFactoryView()
{
    factoryView_ = new QWidget();
    auto* layout = new QVBoxLayout(factoryView_);

    // Create order section
    auto* createGroup = new QGroupBox("Create New Order", factoryView_);
    createGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #bdc3c7; border-radius: 5px; margin: 5px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }");
    auto* createLayout = new QVBoxLayout(createGroup);

    titleEdit_ = new QLineEdit(factoryView_);
    titleEdit_->setPlaceholderText("Order title...");
    titleEdit_->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 3px;");
    
    descriptionEdit_ = new QTextEdit(factoryView_);
    descriptionEdit_->setPlaceholderText("Order description...");
    descriptionEdit_->setMaximumHeight(80);
    descriptionEdit_->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 3px;");
    
    createBtn_ = new QPushButton("Create Order", factoryView_);
    createBtn_->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border: none; padding: 10px 20px; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #229954; }");
    connect(createBtn_, &QPushButton::clicked, this, &OrdersPanel::onCreateOrderClicked);

    createLayout->addWidget(new QLabel("Title:"));
    createLayout->addWidget(titleEdit_);
    createLayout->addWidget(new QLabel("Description:"));
    createLayout->addWidget(descriptionEdit_);
    createLayout->addWidget(createBtn_);
    
    layout->addWidget(createGroup);

    // My orders list
    auto* ordersGroup = new QGroupBox("My Orders", factoryView_);
    ordersGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #bdc3c7; border-radius: 5px; margin: 5px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }");
    auto* ordersLayout = new QVBoxLayout(ordersGroup);
    
    factoryOrdersList_ = new QListWidget(factoryView_);
    factoryOrdersList_->setStyleSheet("QListWidget { border: 1px solid #bdc3c7; border-radius: 3px; } QListWidget::item { padding: 8px; border-bottom: 1px solid #ecf0f1; } QListWidget::item:selected { background-color: #3498db; color: white; }");
    connect(factoryOrdersList_, &QListWidget::currentRowChanged, this, &OrdersPanel::onOrderSelectionChanged);
    
    ordersLayout->addWidget(factoryOrdersList_);
    layout->addWidget(ordersGroup);

    viewStack_->addWidget(factoryView_);
}

void OrdersPanel::setupExpertView()
{
    expertView_ = new QWidget();
    auto* layout = new QVBoxLayout(expertView_);

    // Open orders
    auto* openGroup = new QGroupBox("Open Orders", expertView_);
    openGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #bdc3c7; border-radius: 5px; margin: 5px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }");
    auto* openLayout = new QVBoxLayout(openGroup);
    
    openOrdersList_ = new QListWidget(expertView_);
    openOrdersList_->setStyleSheet("QListWidget { border: 1px solid #bdc3c7; border-radius: 3px; } QListWidget::item { padding: 8px; border-bottom: 1px solid #ecf0f1; } QListWidget::item:selected { background-color: #3498db; color: white; }");
    connect(openOrdersList_, &QListWidget::currentRowChanged, this, &OrdersPanel::onOrderSelectionChanged);
    
    openLayout->addWidget(openOrdersList_);
    layout->addWidget(openGroup);

    // Assigned orders
    auto* assignedGroup = new QGroupBox("Assigned to Me", expertView_);
    assignedGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #bdc3c7; border-radius: 5px; margin: 5px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }");
    auto* assignedLayout = new QVBoxLayout(assignedGroup);
    
    assignedOrdersList_ = new QListWidget(expertView_);
    assignedOrdersList_->setStyleSheet("QListWidget { border: 1px solid #bdc3c7; border-radius: 3px; } QListWidget::item { padding: 8px; border-bottom: 1px solid #ecf0f1; } QListWidget::item:selected { background-color: #3498db; color: white; }");
    connect(assignedOrdersList_, &QListWidget::currentRowChanged, this, &OrdersPanel::onOrderSelectionChanged);
    
    assignedLayout->addWidget(assignedOrdersList_);
    layout->addWidget(assignedGroup);

    viewStack_->addWidget(expertView_);
}

void OrdersPanel::setConnection(ClientConn* conn)
{
    conn_ = conn;
    if (conn_) {
        connect(conn_, &ClientConn::packetReceived, this, &OrdersPanel::onOrderPacketReceived);
    }
}

void OrdersPanel::setUserInfo(const QString& username, const QString& role, const QString& token)
{
    username_ = username;
    role_ = role;
    token_ = token;

    // Switch view based on role
    if (role_ == "factory") {
        viewStack_->setCurrentWidget(factoryView_);
    } else if (role_ == "expert") {
        viewStack_->setCurrentWidget(expertView_);
    }
    
    refreshOrders();
}

void OrdersPanel::refreshOrders()
{
    if (!conn_ || token_.isEmpty()) return;
    
    sendOrderRequest("list");
}

void OrdersPanel::onCreateOrderClicked()
{
    QString title = titleEdit_->text().trimmed();
    QString description = descriptionEdit_->toPlainText().trimmed();
    
    if (title.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a title for the order.");
        return;
    }
    
    QJsonObject params;
    params["title"] = title;
    params["description"] = description;
    
    sendOrderRequest("create", params);
    
    // Clear form
    titleEdit_->clear();
    descriptionEdit_->clear();
}

void OrdersPanel::onRefreshClicked()
{
    refreshOrders();
}

void OrdersPanel::onOrderSelectionChanged()
{
    QListWidget* sender = qobject_cast<QListWidget*>(QObject::sender());
    if (!sender) return;
    
    QListWidgetItem* item = sender->currentItem();
    if (!item) {
        detailsGroup_->setVisible(false);
        return;
    }
    
    QJsonObject order = item->data(Qt::UserRole).toJsonObject();
    currentOrder_ = order;
    
    updateOrderDetails(order);
    enableOrderActions(order);
    detailsGroup_->setVisible(true);
}

void OrdersPanel::onAcceptOrderClicked()
{
    if (currentOrder_.isEmpty()) return;
    
    QJsonObject params;
    params["id"] = currentOrder_["id"].toString();
    
    sendOrderRequest("accept", params);
}

void OrdersPanel::onRejectOrderClicked()
{
    if (currentOrder_.isEmpty()) return;
    
    // Note: reject functionality would need to be implemented in server
    QMessageBox::information(this, "Not Implemented", "Reject functionality not yet implemented.");
}

void OrdersPanel::onOpenRoomClicked()
{
    if (currentOrder_.isEmpty()) return;
    
    QString orderId = currentOrder_["id"].toString();
    emit joinRoomRequested(orderId);
}

void OrdersPanel::onCloseOrderClicked()
{
    if (currentOrder_.isEmpty()) return;
    
    QJsonObject params;
    params["id"] = currentOrder_["id"].toString();
    params["status"] = "closed";
    
    sendOrderRequest("status", params);
}

void OrdersPanel::onCancelOrderClicked()
{
    if (currentOrder_.isEmpty()) return;
    
    QJsonObject params;
    params["id"] = currentOrder_["id"].toString();
    params["status"] = "canceled";
    
    sendOrderRequest("status", params);
}

void OrdersPanel::updateOrderDetails(const QJsonObject& order)
{
    orderIdLabel_->setText(order["id"].toString());
    orderTitleLabel_->setText(order["title"].toString());
    orderDescLabel_->setText(order["description"].toString());
    
    QString status = order["status"].toString();
    QString statusText = status;
    if (status == "open") statusText = "🟢 Open";
    else if (status == "assigned") statusText = "🟡 Assigned";
    else if (status == "in_progress") statusText = "🔵 In Progress";
    else if (status == "closed") statusText = "✅ Closed";
    else if (status == "canceled") statusText = "❌ Canceled";
    orderStatusLabel_->setText(statusText);
    
    orderCreatedByLabel_->setText(order["created_by"].toString());
    orderAssignedToLabel_->setText(order["assigned_to"].toString().isEmpty() ? "—" : order["assigned_to"].toString());
    
    QString createdAt = order["created_at"].toString();
    QString updatedAt = order["updated_at"].toString();
    orderCreatedAtLabel_->setText(createdAt.isEmpty() ? "—" : QDateTime::fromString(createdAt, Qt::ISODate).toString("yyyy-MM-dd hh:mm"));
    orderUpdatedAtLabel_->setText(updatedAt.isEmpty() ? "—" : QDateTime::fromString(updatedAt, Qt::ISODate).toString("yyyy-MM-dd hh:mm"));
}

void OrdersPanel::enableOrderActions(const QJsonObject& order)
{
    QString status = order["status"].toString();
    QString createdBy = order["created_by"].toString();
    QString assignedTo = order["assigned_to"].toString();
    
    // Reset all buttons
    acceptBtn_->setVisible(false);
    rejectBtn_->setVisible(false);
    openRoomBtn_->setEnabled(false);
    closeOrderBtn_->setEnabled(false);
    cancelOrderBtn_->setEnabled(false);
    
    if (role_ == "expert") {
        if (status == "open") {
            acceptBtn_->setVisible(true);
            rejectBtn_->setVisible(true);
        }
        if (assignedTo == username_ && (status == "assigned" || status == "in_progress")) {
            openRoomBtn_->setEnabled(true);
        }
    } else if (role_ == "factory") {
        if (createdBy == username_) {
            if (status == "assigned" || status == "in_progress") {
                openRoomBtn_->setEnabled(true);
            }
            if (status != "closed" && status != "canceled") {
                closeOrderBtn_->setEnabled(true);
                cancelOrderBtn_->setEnabled(true);
            }
        }
    }
}

void OrdersPanel::addOrderToList(const QJsonObject& order)
{
    QString status = order["status"].toString();
    QString title = order["title"].toString();
    QString createdBy = order["created_by"].toString();
    QString assignedTo = order["assigned_to"].toString();
    
    QString displayText = QString("[%1] %2").arg(status.toUpper(), title);
    if (!assignedTo.isEmpty()) {
        displayText += QString(" (→ %1)").arg(assignedTo);
    }
    
    auto* item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, order);
    
    if (role_ == "factory") {
        factoryOrdersList_->addItem(item);
    } else if (role_ == "expert") {
        if (status == "open") {
            openOrdersList_->addItem(item);
        } else if (assignedTo == username_) {
            assignedOrdersList_->addItem(item);
        }
    }
}

void OrdersPanel::updateOrderInList(const QJsonObject& order)
{
    // For simplicity, just refresh the entire list
    // In a real application, you'd want to update specific items
    refreshOrders();
}

void OrdersPanel::sendOrderRequest(const QString& op, const QJsonObject& params)
{
    if (!conn_ || token_.isEmpty()) return;
    
    QJsonObject request = params;
    request["op"] = op;
    request["token"] = token_;
    
    conn_->send(MSG_ORDER, request);
}

void OrdersPanel::onOrderPacketReceived(const Packet& p)
{
    if (p.type == MSG_ORDER) {
        QString op = p.json["op"].toString();
        
        if (op == "list") {
            // Clear existing lists
            if (role_ == "factory") {
                factoryOrdersList_->clear();
            } else if (role_ == "expert") {
                openOrdersList_->clear();
                assignedOrdersList_->clear();
            }
            
            // Add orders to appropriate lists
            QJsonArray orders = p.json["orders"].toArray();
            for (const QJsonValue& value : orders) {
                addOrderToList(value.toObject());
            }
        } else if (op == "create" || op == "accept" || op == "status") {
            // Refresh orders to show updated state
            refreshOrders();
        }
    } else if (p.type == MSG_SERVER_EVENT && p.json["kind"].toString() == "order") {
        // Handle real-time order updates
        QString event = p.json["event"].toString();
        QJsonObject order = p.json["order"].toObject();
        
        if (event == "created" || event == "updated" || event == "accepted") {
            updateOrderInList(order);
        }
    }
}