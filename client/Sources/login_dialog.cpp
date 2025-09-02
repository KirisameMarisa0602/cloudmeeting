#include "login_dialog.h"
#include "clientconn.h"
#include "protocol.h"
#include <QTimer>
#include <QApplication>
#include <QThread>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , isRegisterMode_(false)
    , conn_(nullptr)
{
    setWindowTitle("CloudMeeting - Authentication");
    setFixedSize(400, 350);
    setModal(true);
    
    setupUI();
    setMode(false); // Start in login mode
    
    // Create connection object
    conn_ = new ClientConn(this);
}

LoginDialog::~LoginDialog()
{
    if (conn_) {
        conn_->disconnect();
    }
}

void LoginDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Title
    titleLabel_ = new QLabel("Login to CloudMeeting", this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 10px;");
    mainLayout->addWidget(titleLabel_);

    // Form group
    formGroup_ = new QGroupBox(this);
    formGroup_->setStyleSheet("QGroupBox { border: 1px solid #bdc3c7; border-radius: 5px; padding: 10px; }");
    auto* formLayout = new QFormLayout(formGroup_);
    formLayout->setSpacing(10);

    // Username
    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setPlaceholderText("Enter username");
    usernameEdit_->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 3px;");
    formLayout->addRow("Username:", usernameEdit_);

    // Password
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText("Enter password");
    passwordEdit_->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 3px;");
    formLayout->addRow("Password:", passwordEdit_);

    // Confirm password (hidden initially)
    confirmPasswordEdit_ = new QLineEdit(this);
    confirmPasswordEdit_->setEchoMode(QLineEdit::Password);
    confirmPasswordEdit_->setPlaceholderText("Confirm password");
    confirmPasswordEdit_->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 3px;");
    formLayout->addRow("Confirm:", confirmPasswordEdit_);

    // Role selection
    roleCombo_ = new QComboBox(this);
    roleCombo_->addItem("Select Role...", "");
    roleCombo_->addItem("Factory Worker", "factory");
    roleCombo_->addItem("Expert Technician", "expert");
    roleCombo_->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 3px;");
    formLayout->addRow("Role:", roleCombo_);

    mainLayout->addWidget(formGroup_);

    // Progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setVisible(false);
    progressBar_->setStyleSheet("QProgressBar { border: 1px solid #bdc3c7; border-radius: 3px; text-align: center; } QProgressBar::chunk { background-color: #3498db; }");
    mainLayout->addWidget(progressBar_);

    // Status label
    statusLabel_ = new QLabel(this);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setStyleSheet("color: #e74c3c; font-weight: bold;");
    statusLabel_->setWordWrap(true);
    statusLabel_->setVisible(false);
    mainLayout->addWidget(statusLabel_);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    switchModeBtn_ = new QPushButton("Need an account? Register", this);
    switchModeBtn_->setStyleSheet("QPushButton { background: none; border: none; color: #3498db; text-decoration: underline; } QPushButton:hover { color: #2980b9; }");
    switchModeBtn_->setCursor(Qt::PointingHandCursor);
    connect(switchModeBtn_, &QPushButton::clicked, this, &LoginDialog::onModeChanged);

    buttonLayout->addWidget(switchModeBtn_);
    buttonLayout->addStretch();

    registerBtn_ = new QPushButton("Register", this);
    registerBtn_->setStyleSheet("QPushButton { background-color: #27ae60; color: white; border: none; padding: 10px 20px; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #229954; } QPushButton:pressed { background-color: #1e8449; }");
    connect(registerBtn_, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
    buttonLayout->addWidget(registerBtn_);

    loginBtn_ = new QPushButton("Login", this);
    loginBtn_->setStyleSheet("QPushButton { background-color: #3498db; color: white; border: none; padding: 10px 20px; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #2980b9; } QPushButton:pressed { background-color: #21618c; }");
    loginBtn_->setDefault(true);
    connect(loginBtn_, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    buttonLayout->addWidget(loginBtn_);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();

    // Connect Enter key
    connect(usernameEdit_, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(confirmPasswordEdit_, &QLineEdit::returnPressed, this, &LoginDialog::onRegisterClicked);
}

void LoginDialog::setMode(bool isRegisterMode)
{
    isRegisterMode_ = isRegisterMode;
    
    if (isRegisterMode) {
        titleLabel_->setText("Register for CloudMeeting");
        confirmPasswordEdit_->setVisible(true);
        roleCombo_->setVisible(true);
        loginBtn_->setVisible(false);
        registerBtn_->setVisible(true);
        switchModeBtn_->setText("Already have an account? Login");
    } else {
        titleLabel_->setText("Login to CloudMeeting");
        confirmPasswordEdit_->setVisible(false);
        roleCombo_->setVisible(false);
        loginBtn_->setVisible(true);
        registerBtn_->setVisible(false);
        switchModeBtn_->setText("Need an account? Register");
    }
    
    statusLabel_->setVisible(false);
    usernameEdit_->setFocus();
}

void LoginDialog::onModeChanged()
{
    setMode(!isRegisterMode_);
}

void LoginDialog::onLoginClicked()
{
    QString username = usernameEdit_->text().trimmed();
    QString password = passwordEdit_->text();

    if (username.isEmpty() || password.isEmpty()) {
        statusLabel_->setText("Please enter username and password");
        statusLabel_->setVisible(true);
        return;
    }

    if (performAuth("login", username, password)) {
        accept();
    }
}

void LoginDialog::onRegisterClicked()
{
    QString username = usernameEdit_->text().trimmed();
    QString password = passwordEdit_->text();
    QString confirmPassword = confirmPasswordEdit_->text();
    QString role = roleCombo_->currentData().toString();

    if (username.isEmpty() || password.isEmpty()) {
        statusLabel_->setText("Please enter username and password");
        statusLabel_->setVisible(true);
        return;
    }

    if (password != confirmPassword) {
        statusLabel_->setText("Passwords do not match");
        statusLabel_->setVisible(true);
        return;
    }

    if (role.isEmpty()) {
        statusLabel_->setText("Please select a role");
        statusLabel_->setVisible(true);
        return;
    }

    if (performAuth("register", username, password, role)) {
        // After successful registration, switch to login mode
        setMode(false);
        statusLabel_->setText("Registration successful! Please login.");
        statusLabel_->setStyleSheet("color: #27ae60; font-weight: bold;");
        statusLabel_->setVisible(true);
    }
}

bool LoginDialog::performAuth(const QString& op, const QString& username, const QString& password, const QString& role)
{
    statusLabel_->setVisible(false);
    progressBar_->setVisible(true);
    progressBar_->setRange(0, 0); // Indeterminate progress
    
    // Disable form during authentication
    formGroup_->setEnabled(false);
    loginBtn_->setEnabled(false);
    registerBtn_->setEnabled(false);
    
    bool success = false;
    QString errorMsg;
    
    // Connect to server
    conn_->connectTo("127.0.0.1", 9000);
    
    // Wait for connection
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(5000); // 5 second timeout
    
    bool connectionEstablished = false;
    bool responseReceived = false;
    
    connect(conn_, &ClientConn::connected, this, [&]() {
        connectionEstablished = true;
        
        // Send authentication request
        QJsonObject authReq;
        authReq["op"] = op;
        authReq["username"] = username;
        authReq["password"] = password;
        if (!role.isEmpty()) {
            authReq["role"] = role;
        }
        
        conn_->send(MSG_AUTH, authReq);
    });
    
    connect(conn_, &ClientConn::disconnected, this, [&]() {
        if (!responseReceived) {
            errorMsg = "Connection lost";
            responseReceived = true;
            timeout.stop();
        }
    });
    
    connect(conn_, &ClientConn::packetArrived, this, [&](const Packet& p) {
        if (p.type == MSG_SERVER_EVENT && p.json["kind"].toString() == "auth") {
            responseReceived = true;
            timeout.stop();
            
            QString event = p.json["event"].toString();
            if (event == "ok") {
                success = true;
                if (op == "login") {
                    result_.success = true;
                    result_.token = p.json["token"].toString();
                    result_.username = username;
                    QJsonObject user = p.json["user"].toObject();
                    result_.role = user["role"].toString();
                }
            } else {
                errorMsg = p.json["message"].toString();
            }
        }
    });
    
    connect(&timeout, &QTimer::timeout, this, [&]() {
        responseReceived = true;
        if (!connectionEstablished) {
            errorMsg = "Connection timeout";
        } else {
            errorMsg = "Server response timeout";
        }
    });
    
    // Simple event loop to wait for response
    while (!responseReceived) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    
    conn_->disconnectFromHost();
    
    // Re-enable form
    progressBar_->setVisible(false);
    formGroup_->setEnabled(true);
    loginBtn_->setEnabled(true);
    registerBtn_->setEnabled(true);
    
    if (!success) {
        statusLabel_->setText(errorMsg.isEmpty() ? "Authentication failed" : errorMsg);
        statusLabel_->setStyleSheet("color: #e74c3c; font-weight: bold;");
        statusLabel_->setVisible(true);
        return false;
    }
    
    return true;
}