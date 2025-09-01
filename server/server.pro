QT -= gui
QT += core network sql

CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = server

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/roomhub.cpp \
    src/udprelay.cpp

HEADERS += \
    src/roomhub.h \
    src/udprelay.h

# 引入共享协议（此 pri 内部已经把 protocol.h/.cpp 加入 HEADERS/SOURCES）
COMMON_DIR = $$PWD/../common
include($$COMMON_DIR/common.pri)

isEmpty(COMMON_DIR) {
    error("common.pri requires COMMON_DIR to be set by includer")
}

INCLUDEPATH += $$PWD/common
HEADERS += common/protocol.h
SOURCES += common/protocol.cpp

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
