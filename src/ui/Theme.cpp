#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>

#include <algorithm>

namespace duality::Theme {

namespace {

// The application-wide type family, bundled in resources.qrc.
const char *const kFontFamily = "Pixelify Sans";

// Load the Pixelify Sans weights from resources once, so every widget (and the
// QPainter-drawn plot text) can use them.
void loadFonts()
{
    static bool loaded = false;
    if (loaded)
        return;
    loaded = true;
    for (const char *weight : {"Regular", "Medium", "SemiBold", "Bold"}) {
        QFontDatabase::addApplicationFont(
            QStringLiteral(":/fonts/PixelifySans-%1.ttf")
                .arg(QLatin1String(weight)));
    }
}

// The full-size touchscreen style: generous padding and large hit targets,
// tuned for the desktop / large-panel layout.
QString wideStyleSheet()
{
    return QStringLiteral(R"(
* {
    background-color: #000000;
    color: #ffffff;
    border-radius: 0px;
    font-family: "Pixelify Sans";
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
)");
}

// The same monochrome look scaled down for the 480x320 panel: ~11px type,
// tight padding, and hit targets that are still tappable (~26px tall) but small
// enough that a full settings panel fits without dominating the screen.
QString compactStyleSheet()
{
    return QStringLiteral(R"(
* {
    background-color: #000000;
    color: #ffffff;
    border-radius: 0px;
    font-size: 11px;
    font-family: "Pixelify Sans";
    selection-background-color: #ffffff;
    selection-color: #000000;
}
QPushButton {
    border: 1px solid #ffffff;
    padding: 4px 8px;
    min-height: 16px;
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
    padding: 3px 4px;
    min-height: 16px;
}
QComboBox::drop-down {
    border-left: 1px solid #ffffff;
    width: 22px;
}
QComboBox::down-arrow {
    image: url(:/assets/arrow-down-white.png);
    width: 11px;
    height: 8px;
}
QComboBox::down-arrow:disabled {
    image: url(:/assets/arrow-down-gray.png);
}
QComboBox QAbstractItemView {
    border: 1px solid #ffffff;
}
QSpinBox, QDoubleSpinBox {
    border: 1px solid #ffffff;
    padding: 3px 26px;
    min-height: 16px;
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
    width: 24px;
    height: 26px;
    border: 1px solid #ffffff;
    background: #000000;
}
QSpinBox::up-button, QDoubleSpinBox::up-button {
    subcontrol-origin: border;
    subcontrol-position: center right;
    width: 24px;
    height: 26px;
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
    width: 13px;
    height: 9px;
}
QSpinBox::up-arrow:pressed, QDoubleSpinBox::up-arrow:pressed {
    image: url(:/assets/arrow-up-black.png);
}
QSpinBox::up-arrow:disabled, QDoubleSpinBox::up-arrow:disabled {
    image: url(:/assets/arrow-up-gray.png);
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: url(:/assets/arrow-down-white.png);
    width: 13px;
    height: 9px;
}
QSpinBox::down-arrow:pressed, QDoubleSpinBox::down-arrow:pressed {
    image: url(:/assets/arrow-down-black.png);
}
QSpinBox::down-arrow:disabled, QDoubleSpinBox::down-arrow:disabled {
    image: url(:/assets/arrow-down-gray.png);
}
QCheckBox::indicator, QGroupBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #ffffff;
}
QCheckBox::indicator:checked, QGroupBox::indicator:checked {
    background-color: #ffffff;
}
QGroupBox {
    border: 1px solid #ffffff;
    margin-top: 8px;
    padding-top: 4px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 6px;
}
QDockWidget {
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}
QDockWidget::title {
    background: #ffffff;
    color: #000000;
    border: 1px solid #ffffff;
    padding: 3px;
    text-align: left;
}
QTreeWidget, QTableWidget, QListWidget {
    border: 1px solid #ffffff;
}
QHeaderView::section {
    border: 1px solid #ffffff;
    padding: 3px;
}
QToolBar {
    border-bottom: 1px solid #ffffff;
    spacing: 4px;
    padding: 3px;
}
QToolButton {
    border: 1px solid #ffffff;
    background: #000000;
    padding: 5px 10px;
    min-height: 16px;
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
QScrollBar:vertical {
    border: 1px solid #ffffff;
    background: #000000;
    width: 20px;
    margin: 0px;
}
QScrollBar::handle:vertical {
    background: #ffffff;
    min-height: 24px;
}
QScrollBar:horizontal {
    border: 1px solid #ffffff;
    background: #000000;
    height: 20px;
    margin: 0px;
}
QScrollBar::handle:horizontal {
    background: #ffffff;
    min-width: 24px;
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0px;
    height: 0px;
    background: none;
    border: none;
}
QScrollBar::add-page, QScrollBar::sub-page {
    background: none;
}
QSlider::groove:horizontal {
    border: 1px solid #ffffff;
    height: 4px;
}
QSlider::handle:horizontal {
    background: #ffffff;
    width: 18px;
    margin: -9px 0;
}
QTabBar::tab {
    border: 1px solid #ffffff;
    padding: 5px 8px;
}
QTabBar::tab:selected {
    background: #ffffff;
    color: #000000;
}
)");
}

} // namespace

void apply(QApplication &app, bool compact)
{
    loadFonts();
    // Set the default font so QPainter-drawn text (spectrum/waterfall axes)
    // uses Pixelify Sans too; the stylesheet's font-family covers styled
    // widgets.
    QFont appFont{QString::fromLatin1(kFontFamily)};
    appFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(appFont);
    app.setStyleSheet(compact ? compactStyleSheet() : wideStyleSheet());
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
