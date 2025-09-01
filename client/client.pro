QT += core gui widgets network multimedia multimediawidgets
CONFIG += c++17
# 如需调试控制台输出可解开：CONFIG += console

TEMPLATE = app
TARGET = cloudmeeting-client

INCLUDEPATH += $$PWD/Headers $$PWD/Headers/comm

# 递归纳入所有头/源
HEADERS += \
    $$files($$PWD/Headers/*.h, true)

SOURCES += \
    $$files($$PWD/Sources/*.cpp, true)

# 只纳入实际 UI 目录，避免重复 uic 生成
FORMS += \
    $$files($$PWD/Forms/*.ui, true)

# 若有资源 .qrc，可取消注释
# RESOURCES += \
#     $$files($$PWD/Resources/*.qrc, true)

QMAKE_CXXFLAGS += -Wall
