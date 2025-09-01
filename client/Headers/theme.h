#pragma once
#include <QWidget>
#include <QString>

namespace Theme {

inline QString commonQss() {
    return QStringLiteral(
        "QWidget { font-family: 'Microsoft YaHei', 'Noto Sans CJK', sans-serif; color:#e5e7eb; }"
        "QPushButton { padding: 6px 12px; border-radius: 8px; border: 1px solid rgba(255,255,255,0.18); background: rgba(255,255,255,0.06); }"
        "QPushButton:hover { filter: brightness(1.06); }"
        "QPushButton:pressed { filter: brightness(0.95); }"
        "QLineEdit, QComboBox, QTextEdit { padding: 6px 8px; border: 1px solid rgba(255,255,255,0.18); border-radius: 6px; background: rgba(0,0,0,0.3); color: #e5e7eb; }"
        "QHeaderView::section { background: rgba(255,255,255,0.08); color: #e5e7eb; border: 0; padding: 6px 10px; }"
        "QTableView, QTableWidget { background: rgba(0,0,0,0.35); alternate-background-color: rgba(255,255,255,0.04); selection-background-color: rgba(255,255,255,0.18); selection-color: #f9fafb; gridline-color: rgba(229,231,235,0.12); border: 1px solid rgba(255,255,255,0.10); border-radius: 8px; }"
        "QGroupBox { border: 1px solid rgba(255,255,255,0.12); border-radius: 8px; margin-top: 8px; padding: 8px; }"
    );
}

// 专家端（蓝/浅蓝）
inline QString expertQss() {
    return QStringLiteral(
        "QMainWindow, QWidget { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #0f172a, stop:0.5 #12223c, stop:1 #0b1325); }"
        "QTabWidget::pane { border: 1px solid #1e40af; border-top: 2px solid #3b82f6; border-radius: 10px; padding-top: 8px; background: rgba(23,37,84,0.32); }"
        "QTabBar::tab { padding: 8px 16px; margin: 0 4px; border-top-left-radius: 8px; border-top-right-radius: 8px; color: #e5edff; background: rgba(59,130,246,0.12); border: 1px solid #1e40af; }"
        "QTabBar::tab:selected { background: #3b82f6; color: #ffffff; border: 1px solid #3b82f6; }"
        "QTabBar::tab:hover { background: rgba(147,197,253,0.35); }"
        "QPushButton { background: rgba(59,130,246,0.20); border: 1px solid #3b82f6; color: #e6f0ff; }"
        "QLineEdit, QComboBox, QTextEdit { background: rgba(30,58,138,0.30); border: 1px solid rgba(147,197,253,0.45); }"
        "QTableView, QTableWidget { border: 1px solid rgba(59,130,246,0.50); background: rgba(2,6,23,0.35); }"
        "QGroupBox { border: 1px solid rgba(59,130,246,0.45); }"
    );
}

// 工厂端（绿/浅绿）
inline QString factoryQss() {
    return QStringLiteral(
        "QMainWindow, QWidget { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #0b1410, stop:0.5 #0f2017, stop:1 #0a1712); }"
        "QTabWidget::pane { border: 1px solid #166534; border-top: 2px solid #22c55e; border-radius: 10px; padding-top: 8px; background: rgba(9,39,26,0.28); }"
        "QTabBar::tab { padding: 8px 16px; margin: 0 4px; border-top-left-radius: 8px; border-top-right-radius: 8px; color: #e8ffee; background: rgba(34,197,94,0.14); border: 1px solid #166534; }"
        "QTabBar::tab:selected { background: #22c55e; color: #0b1410; border: 1px solid #22c55e; }"
        "QTabBar::tab:hover { background: rgba(134,239,172,0.35); }"
        "QPushButton { background: rgba(34,197,94,0.20); border: 1px solid #22c55e; color: #e8ffee; }"
        "QLineEdit, QComboBox, QTextEdit { background: rgba(5,46,22,0.30); border: 1px solid rgba(134,239,172,0.45); }"
        "QTableView, QTableWidget { border: 1px solid rgba(34,197,94,0.50); background: rgba(2,24,15,0.32); }"
        "QGroupBox { border: 1px solid rgba(34,197,94,0.45); }"
    );
}

inline void applyExpertTheme(QWidget* w) { if (w) w->setStyleSheet(commonQss() + expertQss()); }
inline void applyFactoryTheme(QWidget* w) { if (w) w->setStyleSheet(commonQss() + factoryQss()); }

} // namespace Theme
