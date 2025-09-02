#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include <QProgressBar>
#include "clientconn.h"
#include "protocol.h"

class LoginDialog;
class OrdersPanel;
class VideoTile;
class AnnotCanvas;
class AudioChat;
class ScreenShare;
class UdpMediaClient;

class NewMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit NewMainWindow(QWidget *parent = nullptr);
    ~NewMainWindow();

private slots:
    void onLoginClicked();
    void onLogoutClicked();
    void onJoinRoom(const QString& orderId);
    void onLeaveRoom();
    void onPacketReceived(const Packet& p);
    void onConnectionStateChanged();
    void onSendChatMessage();
    void onHeartbeat();

private:
    enum Page {
        LoginPage,
        OrdersPage,
        SessionPage
    };

    void setupUI();
    void createMenuBar();
    void createStatusBar();
    void createChatDock();
    void createSessionPage();
    
    void showPage(Page page);
    void updateStatusBar();
    void addChatMessage(const QString& sender, const QString& message, bool isSystem = false);
    void startHeartbeat();
    void stopHeartbeat();
    
    bool authenticateUser();
    void initializeSession(const QString& token, const QString& username, const QString& role);
    void cleanupSession();

private:
    // Core components
    QStackedWidget* stackedWidget_;
    ClientConn* conn_;
    QTimer* heartbeatTimer_;
    
    // Authentication state
    QString authToken_;
    QString username_;
    QString role_;
    bool isAuthenticated_;
    
    // Pages
    QWidget* loginPage_;
    QWidget* ordersPage_;
    QWidget* sessionPage_;
    
    // Login page components
    QPushButton* loginBtn_;
    QLabel* statusLabel_;
    QProgressBar* progressBar_;
    
    // Orders page components
    OrdersPanel* ordersPanel_;
    QPushButton* logoutBtn_;
    
    // Session page components (simplified video session)
    QWidget* videoArea_;
    QLabel* sessionStatusLabel_;
    QPushButton* leaveRoomBtn_;
    QString currentRoomId_;
    
    // Chat dock
    QDockWidget* chatDock_;
    QTextEdit* chatDisplay_;
    QLineEdit* chatInput_;
    QPushButton* chatSendBtn_;
    
    // Status bar
    QLabel* connectionStatus_;
    QLabel* userInfoLabel_;
    QLabel* roomInfoLabel_;
    
    // Media components (simplified for now)
    AudioChat* audio_;
    ScreenShare* share_;
    UdpMediaClient* udp_;
};