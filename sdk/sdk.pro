TEMPLATE = lib
CONFIG += staticlib c++17
TARGET = sdk

QT += core network

INCLUDEPATH += include ../commonlib/include

HEADERS += \
    include/cmsdk.h \
    include/authapi.h \
    include/orderapi.h

SOURCES += \
    src/authapi.cpp \
    src/orderapi.cpp

target.path = $$PWD/lib
INSTALLS += target