#ifndef CLIENT_FACTORY_H
#define CLIENT_FACTORY_H

#include <QWidget>
#include "comm/commwidget.h"
#include <QVector>
#include <client_expert.h>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientFactory; }
QT_END_NAMESPACE

namespace Ui {
class ClientFactory;
}

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
    void onOrderDoubleClicked(int row, int column); // 新增：双击详情

private:
    Ui::ClientFactory *ui;
    QVector<OrderInfo> orders;
    bool deletingOrder = false;
    void refreshOrders();
    void updateTabEnabled();
    void sendCreateOrder(const QString& title, const QString& desc);

    // UI 装饰与工具
    void applyRoleUi();
    void decorateOrdersTable();

    CommWidget* commWidget_ = nullptr;
};

#endif // CLIENT_FACTORY_H
