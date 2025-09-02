TEMPLATE = app
TARGET   = cloudmeeting-server

QT += core network sql
CONFIG += console c++17
QMAKE_CXXFLAGS += -std=c++17
macx: CONFIG -= app_bundle

INCLUDEPATH += $$PWD/../common

HEADERS += \
    $$PWD/src/roomhub.h

SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/roomhub.cpp \
    $$PWD/../common/protocol.cpp
