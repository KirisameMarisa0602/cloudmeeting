#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <QJsonObject>
#include <QString>

// 统一的服务器地址（Qt5.12.8 环境下保持简单函数，避免全局变量未用告警）
inline const char* serverHost() { return "127.0.0.1"; }
inline quint16     serverPort() { return 5555; }

// JSON Lines over TCP 的简单请求助手
// 返回 true 表示成功拿到 JSON 响应；reply 内有 {"ok":bool, "msg":string, ...}
bool sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg = nullptr);

#endif // NET_UTIL_H
