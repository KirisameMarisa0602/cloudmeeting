#pragma once
#include <QtWidgets>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include "mainwindow.h"

// 右侧在线聊天 + 左侧仅视频（隐藏 MainWindow 内置的聊天控件）
class CommWidget : public QWidget {
    Q_OBJECT
public:
    explicit CommWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        // 顶部信息条
        topBar_ = new QFrame(this);
        topBar_->setObjectName("commTopBar");
        auto topLay = new QHBoxLayout(topBar_);
        topLay->setContentsMargins(10, 6, 10, 6);
        labHost_ = new QLabel("Host: -", topBar_);
        labRoom_ = new QLabel("Room: -", topBar_);
        labUser_ = new QLabel("User: -", topBar_);
        labPort_ = new QLabel("Port: -", topBar_);
        for (auto* l : {labHost_, labRoom_, labUser_, labPort_}) {
            l->setTextInteractionFlags(Qt::TextSelectableByMouse);
            topLay->addWidget(l);
            topLay->addSpacing(12);
        }
        topLay->addStretch(1);

        // 左侧视频（嵌入 MainWindow），但隐藏其自带聊天控件
        mw_ = new MainWindow(nullptr);
        videoContainer_ = new QFrame(this);
        videoContainer_->setObjectName("videoArea");
        auto vLay = new QVBoxLayout(videoContainer_);
        vLay->setContentsMargins(6,6,6,6);
        vLay->addWidget(mw_);
        hideLegacyChatControls();

        // 右侧聊天面板
        chatView_ = new QTextBrowser(this);
        chatView_->setObjectName("chatView");
        chatView_->setPlaceholderText("聊天记录…");
        chatInput_ = new QLineEdit(this);
        chatInput_->setPlaceholderText("按 Enter 发送消息…");
        QPushButton* btnSend = new QPushButton("发送", this);
        btnSend->setObjectName("btnSendChat");

        auto chatInputBar = new QHBoxLayout;
        chatInputBar->addWidget(chatInput_, 1);
        chatInputBar->addWidget(btnSend);

        QWidget* chatPanel = new QWidget(this);
        auto chatLay = new QVBoxLayout(chatPanel);
        chatLay->setContentsMargins(6,6,6,6);
        chatLay->addWidget(chatView_, 1);
        chatLay->addLayout(chatInputBar, 0);

        splitter_ = new QSplitter(Qt::Horizontal, this);
        splitter_->addWidget(videoContainer_);
        splitter_->addWidget(chatPanel);
        splitter_->setStretchFactor(0, 3);
        splitter_->setStretchFactor(1, 2);

        // 底部工具条（保留占位）
        bottomBar_ = new QFrame(this);
        bottomBar_->setObjectName("commToolbar");
        auto toolLay = new QHBoxLayout(bottomBar_);
        toolLay->setContentsMargins(10, 6, 10, 6);
        btnMic_    = new QToolButton(bottomBar_); btnMic_->setText("麦克风"); btnMic_->setCheckable(true); btnMic_->setChecked(true);
        btnCam_    = new QToolButton(bottomBar_); btnCam_->setText("摄像头"); btnCam_->setCheckable(true); btnCam_->setChecked(true);
        btnScreen_ = new QToolButton(bottomBar_); btnScreen_->setText("共享屏幕"); btnScreen_->setCheckable(true); btnScreen_->setChecked(false);
        toolLay->addWidget(btnMic_);
        toolLay->addWidget(btnCam_);
        toolLay->addWidget(btnScreen_);
        toolLay->addStretch(1);

        // 总布局
        auto mainLay = new QVBoxLayout(this);
        mainLay->setContentsMargins(0,0,0,0);
        mainLay->addWidget(topBar_, 0);
        mainLay->addWidget(splitter_, 1);
        mainLay->addWidget(bottomBar_, 0);

        // 右侧聊天交互
        connect(btnSend, &QPushButton::clicked, this, &CommWidget::sendChat);
        connect(chatInput_, &QLineEdit::returnPressed, this, &CommWidget::sendChat);
    }

    // 设置连接信息：调用后会自动连接并加入房间
    void setConnectionInfo(const QString& host, quint16 port, const QString& room, const QString& username) {
        host_ = host; port_ = port; room_ = room; user_ = username;
        labHost_->setText("Host: " + host_);
        labPort_->setText("Port: " + QString::number(port_));
        labRoom_->setText("Room: " + (room_.isEmpty() ? "-" : room_));
        labUser_->setText("User: " + (user_.isEmpty() ? "-" : user_));
        connectAndJoin();
    }

private slots:
    void onConnected() {
        appendSystem("服务器连接成功");
        // 连接成功后立即加入房间
        if (!room_.isEmpty() && !user_.isEmpty()) {
            QJsonObject req{{"action","chat_join"},{"room",room_},{"username",user_}};
            writeJson(req);
        }
    }
    void onReadyRead() {
        while (sock_ && sock_->canReadLine()) {
            QByteArray line = sock_->readLine().trimmed();
            if (line.isEmpty()) continue;
            QJsonParseError pe{};
            QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
            if (pe.error != QJsonParseError::NoError || !doc.isObject()) continue;
            QJsonObject o = doc.object();

            const QString action = o.value("action").toString();
            if (action == "chat_broadcast") {
                const bool system = o.value("system").toBool(false);
                const QString from = o.value("from").toString();
                const QString text = o.value("text").toString();
                if (system) appendSystem(text);
                else appendChat(from, text);
            }
        }
    }
    void onDisconnected() {
        appendSystem("与服务器断开连接");
    }

private:
    // ————— UI —————
    QFrame* topBar_ = nullptr;
    QLabel *labHost_ = nullptr, *labRoom_ = nullptr, *labUser_ = nullptr, *labPort_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QFrame* videoContainer_ = nullptr;
    QTextBrowser* chatView_ = nullptr;
    QLineEdit* chatInput_ = nullptr;
    QFrame* bottomBar_ = nullptr;
    QToolButton *btnMic_ = nullptr, *btnCam_ = nullptr, *btnScreen_ = nullptr;
    MainWindow* mw_ = nullptr;

    // ————— 网络 —————
    QTcpSocket* sock_ = nullptr;
    QString host_, room_, user_;
    quint16 port_ = 0;

    void connectAndJoin() {
        if (sock_) { sock_->disconnect(this); sock_->deleteLater(); sock_ = nullptr; }
        sock_ = new QTcpSocket(this);
        connect(sock_, &QTcpSocket::connected,    this, &CommWidget::onConnected);
        connect(sock_, &QTcpSocket::readyRead,    this, &CommWidget::onReadyRead);
        connect(sock_, &QTcpSocket::disconnected, this, &CommWidget::onDisconnected);
        sock_->connectToHost(host_, port_);
    }

    void writeJson(const QJsonObject& o) {
        if (!sock_) return;
        QByteArray line = QJsonDocument(o).toJson(QJsonDocument::Compact);
        line += '\n';
        sock_->write(line);
        sock_->flush();
    }

    void sendChat() {
        const QString text = chatInput_->text().trimmed();
        if (text.isEmpty()) return;
        if (!sock_ || room_.isEmpty() || user_.isEmpty()) { appendSystem("尚未连接或未加入房间"); return; }
        QJsonObject req{{"action","chat_msg"},{"room",room_},{"from",user_},{"text",text}};
        writeJson(req);
        chatInput_->clear();
    }

    void appendSystem(const QString& msg) {
        chatView_->append(QString("<span style='color:#9ca3af;'>[系统] %1</span>").arg(msg.toHtmlEscaped()));
    }
    void appendChat(const QString& from, const QString& text) {
        chatView_->append(QString("<b>%1:</b> %2").arg(from.toHtmlEscaped(), text.toHtmlEscaped()));
    }

    // 尽最大努力隐藏 MainWindow 内置聊天控件，仅保留视频/屏幕区域
    void hideLegacyChatControls() {
        // 假定 MainWindow 内部可能存在以下对象名/控件：逐个隐藏，忽略找不到的情况
        const QStringList names = {
            "leHost","hostEdit","lePort","portEdit","leUser","leRoom",
            "btnJoin","btnSend","btnSendText","textSend","teChat","chatLog","chatView",
            "groupChat","frameChat","chatPanel","chatArea"
        };
        for (const QString& n : names) {
            if (auto* w = mw_->findChild<QWidget*>(n)) { w->setVisible(false); w->setMaximumSize(QSize(0,0)); }
        }
        // 还可以通过文本匹配隐藏标签
        for (auto* lab : mw_->findChildren<QLabel*>()) {
            const QString t = lab->text();
            if (t.startsWith("Host") || t.startsWith("User") || t.startsWith("Room") || t.startsWith("Port"))
                { lab->setVisible(false); lab->setMaximumSize(QSize(0,0)); }
        }
    }
};
