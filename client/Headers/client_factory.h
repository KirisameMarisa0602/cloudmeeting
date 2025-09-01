#ifndef CLIENT_FACTORY_H
#define CLIENT_FACTORY_H

#include <QWidget>
#include "comm/commwidget.h"
#include <QVector>
#include <client_expert.h>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientFactory; }
QT_END_NAMESPACE

class ClientFactory : public QWidget
{
    Q_OBJECT
public:
    explicit ClientFactory(QWidget *parent = nullptr);
    ~ClientFactory();

private slots:
    void on_btnNewOrder_clicked();
    void on_btnDeleteOrder_clicked();
    void on_tabChanged(int idx);
    void onSearchOrder();
    void onOrderDoubleClicked(int row, int column);

private:
    Ui::ClientFactory *ui;
    QVector<OrderInfo> orders;
    bool deletingOrder = false;
    CommWidget* commWidget_ = nullptr;
    QLabel* labUserNameCorner_ = nullptr;

    void refreshOrders();
    void updateTabEnabled();
    void sendCreateOrder(const QString& title, const QString& desc);

    void applyRoleUi();
    void decorateOrdersTable();
    void logoutToLogin();
};

#endif // CLIENT_FACTORY_H
