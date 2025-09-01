#pragma once
#include <QString>

namespace UserSession {
    inline QString factoryAccount;
    inline QString factoryUsername;
    inline QString expertAccount;
    inline QString expertUsername;

    inline void setFactory(const QString& account, const QString& username) {
        factoryAccount = account; factoryUsername = username;
    }
    inline void setExpert(const QString& account, const QString& username) {
        expertAccount = account; expertUsername = username;
    }
}
