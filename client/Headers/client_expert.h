#ifndef CLIENT_EXPERT_H
#define CLIENT_EXPERT_H

#include <QWidget>
#include "comm/commwidget.h"
#include <QVector>
#include "shared_types.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ClientExpert; }
QT_END_NAMESPACE

class QLabel;

class ClientExpert : public QWidget
{
    Q_OBJECT
public:
    explicit ClientExpert(QWidget *parent = nullptr);
    ~ClientExpert();

    void setJoinedOrder(bool joined);

private slots:
    void on_btnAccept_clicked();
    void on_btnReject_clicked();
    void on_tabChanged(int idx);
    void onSearchOrder();
    void onOrderDoubleClicked(int row, int column);

private:
    Ui::ClientExpert *ui;
    CommWidget* commWidget_ = nullptr;
    QVector<OrderInfo> orders;
    bool joinedOrder = false;
    QLabel* labUserNameCorner_ = nullptr;

    // Remember selection across refresh for multi-order editing
    int lastSelectedOrderId_ = -1;

    void refreshOrders();
    void updateTabEnabled();
    void sendUpdateOrder(int orderId, const QString& status);

    void applyRoleUi();
    void decorateOrdersTable();
    void showOrderDetailsDialog(const OrderInfo& od);
    void logoutToLogin();

    bool hasMyAcceptedOrder() const;
    const OrderInfo* findOrder(int id) const;
    void restoreSelection();
};

#endif // CLIENT_EXPERT_H
