TEMPLATE = app
TARGET   = cloudmeeting-server

QT += core network sql
CONFIG += console c++17
QMAKE_CXXFLAGS += -std=c++17
macx: CONFIG -= app_bundle

SOURCES += \
    $$PWD/src/main.cpp
