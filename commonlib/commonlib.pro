TEMPLATE = lib
CONFIG += staticlib c++17
TARGET = commonlib

QT += core network

INCLUDEPATH += include

HEADERS += \
    include/protocol.h

SOURCES += \
    src/protocol.cpp

target.path = $$PWD/lib
INSTALLS += target