QT += core gui widgets multimedia network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++11
DEFINES += QT_DEPRECATED_WARNINGS
TEMPLATE = app
TARGET = client

INCLUDEPATH += Headers
INCLUDEPATH += Headers/comm
INCLUDEPATH += Sources
INCLUDEPATH += Sources/comm
INCLUDEPATH += Forms
INCLUDEPATH += Resources

HEADERS += \
    Headers/client_factory.h \
    Headers/client_expert.h \
    Headers/login.h \
    Headers/regist.h \
    Headers/protocol.h \
    Headers/comm/commwidget.h \
    Headers/comm/mainwindow.h \
    Headers/comm/annot.h \
    Headers/comm/annotcanvas.h \
    Headers/comm/audiochat.h \
    Headers/comm/clientconn.h \
    Headers/comm/screenshare.h \
    Headers/comm/udpmedia.h \
    Headers/comm/volume_popup.h

SOURCES += \
    Sources/client_factory.cpp \
    Sources/client_expert.cpp \
    Sources/login.cpp \
    Sources/regist.cpp \
    Sources/protocol.cpp \
    Sources/main.cpp \
    Sources/comm/commwidget.cpp \
    Sources/comm/mainwindow.cpp \
    Sources/comm/annot.cpp \
    Sources/comm/annotcanvas.cpp \
    Sources/comm/audiochat.cpp \
    Sources/comm/clientconn.cpp \
    Sources/comm/screenshare.cpp \
    Sources/comm/udpmedia.cpp \
    Sources/comm/volume_popup.cpp

FORMS += \
    Forms/client_factory.ui \
    Forms/client_expert.ui \
    Forms/login.ui \
    Forms/regist.ui

RESOURCES += Resources/resources.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
