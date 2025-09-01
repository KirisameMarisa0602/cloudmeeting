#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <QString>

// 通用工单结构，供专家端/工厂端共享
struct OrderInfo {
    int id = 0;
    QString title;
    QString desc;
    QString status;
    QString publisher; // 工厂端用户名
    QString accepter;  // 专家端用户名
};

#endif // SHARED_TYPES_H
