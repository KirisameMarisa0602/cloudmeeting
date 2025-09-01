#pragma once
#include <QWidget>
#include <QString>

namespace Theme {

// 通用：字体与基础控件
inline QString commonQss() {
    return QStringLiteral(
        "QWidget { font-family: 'Microsoft YaHei', 'Noto Sans CJK', sans-serif; }"
        "QPushButton {"
        "  padding: 6px 12px;"
        "  border-radius: 8px;"
        "  border: 1px solid rgba(0,0,0,0.18);"
        "  background: rgba(255,255,255,0.04);"
        "}"
        "QPushButton:hover { filter: brightness(1.05); }"
        "QPushButton:pressed { filter: brightness(0.95); }"
        "QLineEdit, QComboBox, QTextEdit {"
        "  padding: 6px 8px;"
        "  border: 1px solid rgba(255,255,255,0.16);"
        "  border-radius: 6px;"
        "  background: rgba(0,0,0,0.25);"
        "  color: #e5e7eb;"
        "}"
        "QTabBar::tab {"
        "  padding: 8px 16px;"
        "  margin: 0 4px;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "  color: #e5e7eb;"
        "}"
        "QTableView, QTableWidget {"
        "  background: rgba(0,0,0,0.35);"
        "  color: #e5e7eb;"
        "  alternate-background-color: rgba(255,255,255,0.04);"
        "  selection-background-color: rgba(59,130,246,0.22);"
        "  selection-color: #f9fafb;"
        "  gridline-color: rgba(229,231,235,0.12);"
        "  border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px;"
        "}"
        "QHeaderView::section {"
        "  background: rgba(255,255,255,0.08);"
        "  color: #e5e7eb;"
        "  border: 0;"
        "  padding: 6px 10px;"
        "}"
        "QScrollBar:vertical { width: 10px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.25); border-radius: 4px; }"
        "QScrollBar:horizontal { height: 10px; background: transparent; }"
        "QScrollBar::handle:horizontal { background: rgba(255,255,255,0.25); border-radius: 4px; }"
        "QGroupBox {"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 8px;"
        "  margin-top: 8px;"
        "  padding: 8px;"
        "}"
    );
}

// 专家端（蓝/浅蓝基调）
inline QString expertQss() {
    // 主色：#3b82f6 亮色：#93c5fd 深边：#1e40af
    return QStringLiteral(
        "QMainWindow, QWidget {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #0f172a, stop:0.5 #11223a, stop:1 #0b1222);"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #1e40af;"
        "  border-top: 2px solid #3b82f6;"
        "  border-radius: 10px;"
        "  padding-top: 8px;"
        "  background: rgba(23,37,84,0.35);"
        "}"
        "QTabBar::tab {"
        "  background: rgba(59,130,246,0.10);"
        "  border: 1px solid #1e40af;"
        "}"
        "QTabBar::tab:selected {"
        "  background: rgba(59,130,246,0.25);"
        "  border: 1px solid #3b82f6;"
        "}"
        "QTabBar::tab:hover {"
        "  background: rgba(147,197,253,0.25);"
        "}"
        "QPushButton {"
        "  background: rgba(59,130,246,0.20);"
        "  border: 1px solid #3b82f6;"
        "  color: #e6f0ff;"
        "}"
        "QLineEdit, QComboBox, QTextEdit {"
        "  background: rgba(30,58,138,0.35);"
        "  border: 1px solid rgba(147,197,253,0.45);"
        "}"
        "QTableView, QTableWidget {"
        "  border: 1px solid rgba(59,130,246,0.45);"
        "  background: rgba(2,6,23,0.35);"
        "}"
        "QGroupBox { border: 1px solid rgba(59,130,246,0.45); }"
    );
}

// 工厂端（绿/浅绿基调）
inline QString factoryQss() {
    // 主色：#22c55e 亮色：#86efac 深边：#166534
    return QStringLiteral(
        "QMainWindow, QWidget {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #0b1410, stop:0.5 #0f2017, stop:1 #0a1712);"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #166534;"
        "  border-top: 2px solid #22c55e;"
        "  border-radius: 10px;"
        "  padding-top: 8px;"
        "  background: rgba(9,39,26,0.30);"
        "}"
        "QTabBar::tab {"
        "  background: rgba(34,197,94,0.12);"
        "  border: 1px solid #166534;"
        "}"
        "QTabBar::tab:selected {"
        "  background: rgba(34,197,94,0.26);"
        "  border: 1px solid #22c55e;"
        "}"
        "QTabBar::tab:hover {"
        "  background: rgba(134,239,172,0.30);"
        "}"
        "QPushButton {"
        "  background: rgba(34,197,94,0.20);"
        "  border: 1px solid #22c55e;"
        "  color: #e8ffee;"
        "}"
        "QLineEdit, QComboBox, QTextEdit {"
        "  background: rgba(5,46,22,0.35);"
        "  border: 1px solid rgba(134,239,172,0.45);"
        "}"
        "QTableView, QTableWidget {"
        "  border: 1px solid rgba(34,197,94,0.45);"
        "  background: rgba(2,24,15,0.32);"
        "}"
        "QGroupBox { border: 1px solid rgba(34,197,94,0.45); }"
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
