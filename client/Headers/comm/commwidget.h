#pragma once
#include <QtWidgets>
#include "mainwindow.h"

// 顶部信息条 + 中间视频/聊天 + 底部工具条（纯 UI，不侵入你的媒体控制）
class CommWidget : public QWidget {
    Q_OBJECT
public:
    explicit CommWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        // Top: Host/Room/User/Port
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

        // Center: 左视频(MainWindow) + 右聊天
        mw_ = new MainWindow(nullptr);
        videoContainer_ = new QFrame(this);
        videoContainer_->setObjectName("videoArea");
        auto vLay = new QVBoxLayout(videoContainer_);
        vLay->setContentsMargins(6,6,6,6);
        vLay->addWidget(mw_);

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

        // Bottom toolbar
        bottomBar_ = new QFrame(this);
        bottomBar_->setObjectName("commToolbar");
        auto toolLay = new QHBoxLayout(bottomBar_);
        toolLay->setContentsMargins(10, 6, 10, 6);
        btnMic_    = new QToolButton(bottomBar_); btnMic_->setText("麦克风"); btnMic_->setCheckable(true); btnMic_->setChecked(true);
        btnCam_    = new QToolButton(bottomBar_); btnCam_->setText("摄像头"); btnCam_->setCheckable(true); btnCam_->setChecked(true);
        btnScreen_ = new QToolButton(bottomBar_); btnScreen_->setText("共享屏幕"); btnScreen_->setCheckable(true); btnScreen_->setChecked(false);
        btnLeave_  = new QToolButton(bottomBar_); btnLeave_->setText("离开房间");
        for (auto* b : {btnMic_, btnCam_, btnScreen_}) b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        toolLay->addWidget(btnMic_);
        toolLay->addWidget(btnCam_);
        toolLay->addWidget(btnScreen_);
        toolLay->addStretch(1);
        toolLay->addWidget(btnLeave_);

        // Root
        auto main = new QVBoxLayout(this);
        main->setContentsMargins(0,0,0,0);
        main->addWidget(topBar_, 0);
        main->addWidget(splitter_, 1);
        main->addWidget(bottomBar_, 0);
        setLayout(main);

        // Chat 占位发送
        connect(btnSend, &QPushButton::clicked, this, [this]{
            const QString t = chatInput_->text().trimmed();
            if (t.isEmpty()) return;
            appendChatLine(userName_.isEmpty() ? "Me" : userName_, t);
            chatInput_->clear();
        });
        connect(chatInput_, &QLineEdit::returnPressed, btnSend, &QPushButton::click);

        // 轻样式
        setStyleSheet(styleSheet() +
            " QFrame#commTopBar { border: 1px solid rgba(255,255,255,0.14); border-radius: 8px; background: rgba(255,255,255,0.05); }"
            " QFrame#commToolbar { border: 1px solid rgba(255,255,255,0.10); border-radius: 8px; background: rgba(255,255,255,0.04); }"
            " QFrame#videoArea { border: 1px solid rgba(255,255,255,0.10); border-radius: 8px; background: rgba(0,0,0,0.25); }"
            " QTextBrowser#chatView { border: 1px solid rgba(255,255,255,0.10); border-radius: 8px; background: rgba(0,0,0,0.22); }"
        );
    }

    MainWindow* mainWindow() { return mw_; }

    void setConnectionInfo(const QString& host, int port, const QString& room, const QString& user) {
        host_ = host; room_ = room; userName_ = user; port_ = port;
        labHost_->setText(QString("Host: %1").arg(host_));
        labRoom_->setText(QString("Room: %1").arg(room_));
        labUser_->setText(QString("User: %1").arg(userName_));
        labPort_->setText(QString("Port: %1").arg(port_));
    }

public slots:
    void appendChatLine(const QString& from, const QString& text) {
        chatView_->append(QString("<b>%1</b>: %2").arg(from.toHtmlEscaped(), text.toHtmlEscaped()));
    }

private:
    // 顶部
    QFrame* topBar_ = nullptr;
    QLabel *labHost_=nullptr, *labRoom_=nullptr, *labUser_=nullptr, *labPort_=nullptr;
    QString host_, room_, userName_; int port_ = 0;
    // 中间
    QSplitter* splitter_ = nullptr;
    QFrame*   videoContainer_ = nullptr;
    QTextBrowser* chatView_ = nullptr;
    QLineEdit*    chatInput_ = nullptr;
    // 底部
    QFrame* bottomBar_ = nullptr;
    QToolButton *btnMic_=nullptr, *btnCam_=nullptr, *btnScreen_=nullptr, *btnLeave_=nullptr;
    // 嵌入的视频窗口
    MainWindow* mw_ = nullptr;
};
