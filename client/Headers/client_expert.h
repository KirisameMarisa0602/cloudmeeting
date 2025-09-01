#ifndef CLIENT_EXPERT_H
#define CLIENT_EXPERT_H

#include <QWidget>
#include "comm/commwidget.h"
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientExpert; }
QT_END_NAMESPACE

// 工单数据（扩展：发布者/接受者）
struct OrderInfo {
    int id;
    QString title;
    QString desc;
    QString status;
    QString publisher; // 新增：工单发布者（工厂端用户）
    QString accepter;  // 新增：工单接受者（专家端用户）
};

namespace Ui { class ClientExpert; }

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

    void refreshOrders();
    void updateTabEnabled();
    void sendUpdateOrder(int orderId, const QString& status);

    // UI 装饰与工具
    void applyRoleUi();
    void decorateOrdersTable();
    void showOrderDetailsDialog(const OrderInfo& od);

    // 返回登录
    void logoutToLogin();
};

#endif // CLIENT_EXPERT_H
