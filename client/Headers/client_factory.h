#ifndef CLIENT_FACTORY_H
#define CLIENT_FACTORY_H

#include <QWidget>
#include "comm/commwidget.h"
#include <QVector>
#include "shared_types.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ClientFactory; }
QT_END_NAMESPACE

class QLabel;

class ClientFactory : public QWidget
{
    Q_OBJECT
public:
    explicit ClientFactory(QWidget *parent = nullptr);
    ~ClientFactory();

private slots:
    // 使用自定义槽名并手动连接，避免 Qt 自动 on_xxx_clicked 的重复触发
    void handleNewOrderClicked();
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
