TEMPLATE = app
TARGET   = cloudmeeting-server

QT += core network sql
CONFIG += console c++17
QMAKE_CXXFLAGS += -std=c++17
macx: CONFIG -= app_bundle

INCLUDEPATH += ../commonlib/include src

# Link against commonlib
LIBS += -L../commonlib -lcommonlib

HEADERS += \
    src/roomhub.h \
    src/udprelay.h

SOURCES += \
    src/main_consolidated.cpp \
    src/roomhub.cpp \
    src/udprelay.cpp
