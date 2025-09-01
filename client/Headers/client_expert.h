#ifndef CLIENT_EXPERT_H
#define CLIENT_EXPERT_H

#include <QWidget>
#include "comm/commwidget.h"
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientExpert; }
QT_END_NAMESPACE

struct OrderInfo {
    int id;
    QString title;
    QString desc;
    QString status;
};

namespace Ui {
class ClientExpert;
}

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
    void onOrderDoubleClicked(int row, int column); // 新增：双击详情

private:
    Ui::ClientExpert *ui;
    CommWidget* commWidget_ = nullptr;
    QVector<OrderInfo> orders;
    bool joinedOrder = false;

    void refreshOrders();
    void updateTabEnabled();
    void sendUpdateOrder(int orderId, const QString& status);

    // 新增：UI 装饰与工具
    void applyRoleUi();
    void decorateOrdersTable();
    void showOrderDetailsDialog(const OrderInfo& od);
};

#endif // CLIENT_EXPERT_H
