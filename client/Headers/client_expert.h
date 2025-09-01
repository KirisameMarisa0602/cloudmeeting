#ifndef CLIENT_EXPERT_H
#define CLIENT_EXPERT_H

#include <QWidget>
#include "comm/commwidget.h"
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientExpert; }
QT_END_NAMESPACE

// 扩展工单：展示用“发布者/接受者（用户名）”
struct OrderInfo {
    int id;
    QString title;
    QString desc;
    QString status;
    QString publisher; // 工厂端用户名
    QString accepter;  // 专家端用户名
};

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

    void refreshOrders();
    void updateTabEnabled();
    void sendUpdateOrder(int orderId, const QString& status);

    void applyRoleUi();
    void decorateOrdersTable();
    void showOrderDetailsDialog(const OrderInfo& od);
    void logoutToLogin();
};

#endif // CLIENT_EXPERT_H
