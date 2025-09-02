#include "demo_app.h"

DemoApp::DemoApp(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("CloudMeeting Demo");
    resize(800, 600);
    
    // Create central widget
    auto* central = new QWidget(this);
    setCentralWidget(central);
    
    auto* layout = new QVBoxLayout(central);
    
    // Status label
    statusLabel_ = new QLabel("Not authenticated", this);
    statusLabel_->setStyleSheet("padding: 10px; background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 5px; font-weight: bold;");
    layout->addWidget(statusLabel_);
    
    // Stacked widget for different views
    stack_ = new QStackedWidget(this);
    
    // Welcome page
    welcomePage_ = new QWidget(this);
    auto* welcomeLayout = new QVBoxLayout(welcomePage_);
    auto* titleLabel = new QLabel("Welcome to CloudMeeting", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2c3e50; margin: 50px;");
    
    loginBtn_ = new QPushButton("Login / Register", this);
    loginBtn_->setStyleSheet("QPushButton { background-color: #3498db; color: white; border: none; padding: 15px 30px; border-radius: 5px; font-size: 16px; font-weight: bold; } QPushButton:hover { background-color: #2980b9; }");
    loginBtn_->setMaximumWidth(200);
    connect(loginBtn_, &QPushButton::clicked, this, &DemoApp::showLogin);
    
    welcomeLayout->addWidget(titleLabel);
    welcomeLayout->addWidget(loginBtn_, 0, Qt::AlignCenter);
    welcomeLayout->addStretch();
    
    stack_->addWidget(welcomePage_);
    
    // Orders page
    ordersPanel_ = new OrdersPanel(this);
    connect(ordersPanel_, &OrdersPanel::joinRoomRequested, this, &DemoApp::joinRoom);
    stack_->addWidget(ordersPanel_);
    
    layout->addWidget(stack_);
    
    // Control buttons
    auto* buttonLayout = new QHBoxLayout();
    
    logoutBtn_ = new QPushButton("Logout", this);
    logoutBtn_->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border: none; padding: 8px 15px; border-radius: 3px; } QPushButton:hover { background-color: #c0392b; }");
    logoutBtn_->setVisible(false);
    connect(logoutBtn_, &QPushButton::clicked, this, &DemoApp::logout);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(logoutBtn_);
    layout->addLayout(buttonLayout);
    
    // Status bar
    statusBar()->showMessage("Ready");
    
    // Connection
    conn_ = new ClientConn(this);
    
    // Show welcome page initially
    stack_->setCurrentWidget(welcomePage_);
}

void DemoApp::showLogin()
{
    LoginDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        auto result = dialog.getLoginResult();
        if (result.success) {
            // Initialize authenticated session
            authToken_ = result.token;
            username_ = result.username;
            role_ = result.role;
            
            statusLabel_->setText(QString("Authenticated as %1 (%2)").arg(username_, role_));
            statusLabel_->setStyleSheet("padding: 10px; background-color: #d4edda; border: 1px solid #c3e6cb; border-radius: 5px; font-weight: bold; color: #155724;");
            
            // Setup orders panel
            ordersPanel_->setConnection(conn_);
            ordersPanel_->setUserInfo(username_, role_, authToken_);
            
            // Show orders page
            stack_->setCurrentWidget(ordersPanel_);
            logoutBtn_->setVisible(true);
            
            statusBar()->showMessage(QString("Logged in as %1").arg(username_));
        }
    }
}

void DemoApp::logout()
{
    authToken_.clear();
    username_.clear();
    role_.clear();
    
    statusLabel_->setText("Not authenticated");
    statusLabel_->setStyleSheet("padding: 10px; background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 5px; font-weight: bold;");
    
    stack_->setCurrentWidget(welcomePage_);
    logoutBtn_->setVisible(false);
    
    statusBar()->showMessage("Logged out");
}

void DemoApp::joinRoom(const QString& orderId)
{
    QMessageBox::information(this, "Join Room", QString("Joining room for order: %1\n\n(Room joining functionality will be implemented with full MainWindow integration)").arg(orderId));
}