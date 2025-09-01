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

# 引入公共协议（唯一来源）
COMMON_DIR = $$PWD/../common
include($$COMMON_DIR/common.pri)

# 规范构建产物与中间文件目录（保持源码目录干净）
DESTDIR     = $$OUT_PWD/bin
OBJECTS_DIR = $$OUT_PWD/.obj
MOC_DIR     = $$OUT_PWD/.moc
RCC_DIR     = $$OUT_PWD/.rcc
UI_DIR      = $$OUT_PWD/.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
