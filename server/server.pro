TEMPLATE = app
TARGET   = cloudmeeting-server

QT += core network sql
CONFIG += console c++17
QMAKE_CXXFLAGS += -std=c++17
macx: CONFIG -= app_bundle

# 只编译新版主程序，避免将 server/Sources 旧代码编进来
SOURCES += \
    $$PWD/src/main.cpp
