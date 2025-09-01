QT += core network sql
CONFIG += console c++17

TEMPLATE = app
TARGET = cloudmeeting-server

INCLUDEPATH += $$PWD/Headers

HEADERS += \
    $$PWD/Headers/db_bootstrap.h \
    $$PWD/Headers/server_actions.h

SOURCES += \
    $$PWD/Sources/main.cpp \
    $$PWD/Sources/db_bootstrap.cpp \
    $$PWD/Sources/server_actions_example.cpp

QMAKE_CXXFLAGS += -Wall
