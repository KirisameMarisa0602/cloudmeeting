QT += core gui widgets network multimedia multimediawidgets
CONFIG += c++17
# 如需调试控制台输出可解开：CONFIG += console

TEMPLATE = app
TARGET = cloudmeeting-client

INCLUDEPATH += $$PWD/Headers $$PWD/Headers/comm

# 递归纳入所有头/源（Qt 5.12.8）
HEADERS += $$files($$PWD/Headers/*.h, true)
SOURCES += $$files($$PWD/Sources/*.cpp, true)
FORMS   += $$files($$PWD/Forms/*.ui, true)
# RESOURCES 如有可启用：
# RESOURCES += $$files($$PWD/Resources/*.qrc, true)

# 去重，避免同一文件被重复收集导致重复编译/链接
HEADERS   = $$unique(HEADERS)
SOURCES   = $$unique(SOURCES)
FORMS     = $$unique(FORMS)

QMAKE_CXXFLAGS += -Wall

# Link SDK and commonlib
include(../sdk_link.pri)
