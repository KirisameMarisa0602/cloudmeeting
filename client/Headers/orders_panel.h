#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include "protocol.h"

class ClientConn;

class OrdersPanel : public QWidget
{
    Q_OBJECT

public:
    explicit OrdersPanel(QWidget *parent = nullptr);
    ~OrdersPanel();

    void setConnection(ClientConn* conn);
    void setUserInfo(const QString& username, const QString& role, const QString& token);
    void refreshOrders();

signals:
    void joinRoomRequested(const QString& orderId);

private slots:
    void onCreateOrderClicked();
    void onRefreshClicked();
    void onOrderSelectionChanged();
    void onAcceptOrderClicked();
    void onRejectOrderClicked();
    void onOpenRoomClicked();
    void onCloseOrderClicked();
    void onCancelOrderClicked();
    void onOrderPacketReceived(const Packet& p);

private:
    void setupUI();
    void setupFactoryView();
    void setupExpertView();
    void updateOrderDetails(const QJsonObject& order);
    void enableOrderActions(const QJsonObject& order);
    void addOrderToList(const QJsonObject& order);
    void updateOrderInList(const QJsonObject& order);
    void sendOrderRequest(const QString& op, const QJsonObject& params = QJsonObject());

private:
    // UI Components
    QVBoxLayout* mainLayout_;
    QStackedWidget* viewStack_;
    QWidget* factoryView_;
    QWidget* expertView_;
    
    // Factory view components
    QLineEdit* titleEdit_;
    QTextEdit* descriptionEdit_;
    QPushButton* createBtn_;
    QListWidget* factoryOrdersList_;
    
    // Expert view components  
    QListWidget* openOrdersList_;
    QListWidget* assignedOrdersList_;
    
    // Shared components
    QGroupBox* detailsGroup_;
    QLabel* orderIdLabel_;
    QLabel* orderTitleLabel_;
    QLabel* orderDescLabel_;
    QLabel* orderStatusLabel_;
    QLabel* orderCreatedByLabel_;
    QLabel* orderAssignedToLabel_;
    QLabel* orderCreatedAtLabel_;
    QLabel* orderUpdatedAtLabel_;
    
    QPushButton* refreshBtn_;
    QPushButton* acceptBtn_;
    QPushButton* rejectBtn_;
    QPushButton* openRoomBtn_;
    QPushButton* closeOrderBtn_;
    QPushButton* cancelOrderBtn_;
    
    // Data
    ClientConn* conn_;
    QString username_;
    QString role_;
    QString token_;
    QJsonObject currentOrder_;
};