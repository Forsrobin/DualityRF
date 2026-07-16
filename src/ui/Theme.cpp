#include "ui/Theme.h"

#include <QApplication>

#include <algorithm>

namespace duality::Theme {

void apply(QApplication &app)
{
    app.setStyleSheet(QStringLiteral(R"(
* {
    background-color: #000000;
    color: #ffffff;
    border-radius: 0px;
    selection-background-color: #ffffff;
    selection-color: #000000;
}
QPushButton {
    border: 1px solid #ffffff;
    padding: 10px 16px;
    min-height: 20px;
}
QPushButton:pressed, QPushButton:checked {
    background-color: #ffffff;
    color: #000000;
}
QPushButton:disabled {
    border-color: #555555;
    color: #555555;
}
QComboBox, QLineEdit {
    border: 1px solid #ffffff;
    padding: 8px;
    min-height: 20px;
}
QComboBox::drop-down {
    border-left: 1px solid #ffffff;
    width: 36px;
}
QComboBox::down-arrow {
    image: url(:/assets/arrow-down-white.png);
    width: 14px;
    height: 10px;
}
QComboBox::down-arrow:disabled {
    image: url(:/assets/arrow-down-gray.png);
}
QComboBox QAbstractItemView {
    border: 1px solid #ffffff;
}
QSpinBox, QDoubleSpinBox {
    border: 1px solid #ffffff;
    padding: 8px 40px;
    min-height: 20px;
}
QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled,
QLineEdit:disabled {
    border-color: #555555;
    color: #555555;
}
/* Touch-friendly stepper: decrement on the left, increment on the right. */
QSpinBox::down-button, QDoubleSpinBox::down-button {
    subcontrol-origin: border;
    subcontrol-position: center left;
    width: 34px;
    height: 40px;
    border: 1px solid #ffffff;
    background: #000000;
}
QSpinBox::up-button, QDoubleSpinBox::up-button {
    subcontrol-origin: border;
    subcontrol-position: center right;
    width: 34px;
    height: 40px;
    border: 1px solid #ffffff;
    background: #000000;
}
QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,
QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed {
    background: #ffffff;
}
QSpinBox::down-button:disabled, QDoubleSpinBox::down-button:disabled,
QSpinBox::up-button:disabled, QDoubleSpinBox::up-button:disabled {
    border-color: #555555;
}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    image: url(:/assets/arrow-up-white.png);
    width: 18px;
    height: 13px;
}
QSpinBox::up-arrow:pressed, QDoubleSpinBox::up-arrow:pressed {
    image: url(:/assets/arrow-up-black.png);
}
QSpinBox::up-arrow:disabled, QDoubleSpinBox::up-arrow:disabled {
    image: url(:/assets/arrow-up-gray.png);
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: url(:/assets/arrow-down-white.png);
    width: 18px;
    height: 13px;
}
QSpinBox::down-arrow:pressed, QDoubleSpinBox::down-arrow:pressed {
    image: url(:/assets/arrow-down-black.png);
}
QSpinBox::down-arrow:disabled, QDoubleSpinBox::down-arrow:disabled {
    image: url(:/assets/arrow-down-gray.png);
}
QCheckBox::indicator, QGroupBox::indicator {
    width: 20px;
    height: 20px;
    border: 1px solid #ffffff;
}
QCheckBox::indicator:checked, QGroupBox::indicator:checked {
    background-color: #ffffff;
}
QGroupBox {
    border: 1px solid #ffffff;
    margin-top: 12px;
    padding-top: 8px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
}
QDockWidget {
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}
QDockWidget::title {
    background: #ffffff;
    color: #000000;
    border: 1px solid #ffffff;
    padding: 6px;
    text-align: left;
}
QTreeWidget, QTableWidget, QListWidget {
    border: 1px solid #ffffff;
}
QHeaderView::section {
    border: 1px solid #ffffff;
    padding: 6px;
}
QToolBar {
    border-bottom: 1px solid #ffffff;
    spacing: 8px;
    padding: 6px;
}
QToolButton {
    border: 1px solid #ffffff;
    background: #000000;
    padding: 10px 20px;
    min-height: 20px;
}
QToolButton:pressed {
    background: #ffffff;
    color: #000000;
}
/* Active view stands out as an inverted (white) button. */
QToolButton:checked {
    background: #ffffff;
    color: #000000;
}
QStatusBar {
    border-top: 1px solid #ffffff;
}
QScrollBar {
    border: 1px solid #ffffff;
    background: #000000;
    width: 18px;
    height: 18px;
}
QScrollBar::handle {
    background: #ffffff;
    min-height: 24px;
    min-width: 24px;
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0px;
    height: 0px;
}
QSlider::groove:horizontal {
    border: 1px solid #ffffff;
    height: 4px;
}
QSlider::handle:horizontal {
    background: #ffffff;
    width: 24px;
    margin: -12px 0;
}
QTabBar::tab {
    border: 1px solid #ffffff;
    padding: 10px 16px;
}
QTabBar::tab:selected {
    background: #ffffff;
    color: #000000;
}
)"));
}

QRgb spectrumColor(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    // black → blue → cyan → yellow → white
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (t < 0.25f) {
        b = t / 0.25f;
    } else if (t < 0.5f) {
        b = 1.0f;
        g = (t - 0.25f) / 0.25f;
    } else if (t < 0.75f) {
        const float u = (t - 0.5f) / 0.25f;
        r = u;
        g = 1.0f;
        b = 1.0f - u;
    } else {
        const float u = (t - 0.75f) / 0.25f;
        r = 1.0f;
        g = 1.0f;
        b = u;
    }
    return qRgb(static_cast<int>(r * 255), static_cast<int>(g * 255),
                static_cast<int>(b * 255));
}

} // namespace duality::Theme
