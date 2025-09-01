# 若 includer 未显式设置 COMMON_DIR，则默认使用此 pri 所在目录
isEmpty(COMMON_DIR) {
    COMMON_DIR = $$PWD
}

INCLUDEPATH += $$COMMON_DIR
HEADERS += $$COMMON_DIR/protocol.h
SOURCES += $$COMMON_DIR/protocol.cpp
