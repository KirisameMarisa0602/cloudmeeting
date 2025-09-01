#pragma once
#include <QWidget>
#include <QString>

namespace Theme {

// 通用基础样式（字体、输入框、按钮基础）
inline QString commonQss() {
    return QStringLiteral(
        "QWidget { font-family: 'Microsoft YaHei', 'Noto Sans CJK', sans-serif; }"
        "QPushButton {"
        "  padding: 6px 12px;"
        "  border-radius: 6px;"
        "  border: 1px solid rgba(0,0,0,0.12);"
        "}"
        "QLineEdit, QComboBox {"
        "  padding: 4px 6px;"
        "  border: 1px solid #C8CDD5;"
        "  border-radius: 4px;"
        "}"
        "QTabBar::tab { padding: 8px 16px; }"
    );
}

// 专家端（蓝色调）
inline QString expertQss() {
    return QStringLiteral(
        "QMainWindow, QWidget { background: #0f172a; }"       // slate-900
        "QLabel { color: #e5e7eb; }"                           // gray-200
        "QPushButton { background: #2563eb; color: white; }"   // blue-600
        "QPushButton:hover { background: #1d4ed8; }"           // blue-700
        "QLineEdit, QComboBox { background: #111827; color: #e5e7eb; border: 1px solid #374151; }"
        "QTabWidget::pane { border: 1px solid #374151; }"
        "QTabBar::tab { color: #e5e7eb; }"
        "QTabBar::tab:selected { background: #1f2937; }"
    );
}

// 工厂端（绿色调）
inline QString factoryQss() {
    return QStringLiteral(
        "QMainWindow, QWidget { background: #0b1410; }"
        "QLabel { color: #e6f4ea; }"
        "QPushButton { background: #16a34a; color: white; }"
        "QPushButton:hover { background: #15803d; }"
        "QLineEdit, QComboBox { background: #0d1f16; color: #e6f4ea; border: 1px solid #1f3a2a; }"
        "QTabWidget::pane { border: 1px solid #1f3a2a; }"
        "QTabBar::tab { color: #e6f4ea; }"
        "QTabBar::tab:selected { background: #123524; }"
    );
}

inline void applyExpertTheme(QWidget* w) {
    if (!w) return;
    w->setStyleSheet(commonQss() + expertQss());
}

inline void applyFactoryTheme(QWidget* w) {
    if (!w) return;
    w->setStyleSheet(commonQss() + factoryQss());
}

} // namespace Theme
