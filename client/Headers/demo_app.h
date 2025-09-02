#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QMessageBox>
#include <QStatusBar>
#include "login_dialog.h"
#include "orders_panel.h"
#include "clientconn.h"

class DemoApp : public QMainWindow
{
    Q_OBJECT

public:
    DemoApp(QWidget *parent = nullptr);

private slots:
    void showLogin();
    void logout();
    void joinRoom(const QString& orderId);

private:
    QStackedWidget* stack_;
    QWidget* welcomePage_;
    OrdersPanel* ordersPanel_;
    QPushButton* loginBtn_;
    QPushButton* logoutBtn_;
    QLabel* statusLabel_;
    
    ClientConn* conn_;
    QString authToken_;
    QString username_;
    QString role_;
};