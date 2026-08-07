#pragma once

#include "core/Types.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace duality {

class SearchAnimation;
class FoundDisplay;

// Minimal signal-identifier program: pick one of the common bands, set the peak
// level, and press the big START button. It then monitors that band on the
// receiver and, the instant a peak crosses the threshold, reports the
// power-weighted centre frequency in big text. There is no waterfall or FFT —
// just a searching animation, the found frequency, and a RESET to scan again.
// MainWindow runs the RX monitor + peak detection and drives the state through
// setRunning()/showDetection(); the program owns only the parameters and the UI.
class IdentifierProgram : public QWidget {
    Q_OBJECT
public:
    explicit IdentifierProgram(QWidget *parent = nullptr);

    // Rich-text help shown by the top-bar info button.
    static QString infoText();

    // Carrier and sample rate (span) of the selected common band.
    StreamParams streamParams() const;
    // The peak level, in the same dB scale the detector uses.
    float thresholdDb() const;

    // Reflect the scanning state: true switches to the animated searching view
    // (and locks the band picker); false returns to the idle START view.
    void setRunning(bool running);
    // Switch to the "found" view and show the detected centre frequency big,
    // with its peak level driving the lock-on animation's strength meter.
    void showDetection(double frequencyHz, float powerDb);

signals:
    void startRequested();
    void stopRequested();
    // RESET after a detection: re-arm the same scan without a full restart.
    void resetRequested();

private:
    // Idle START page, searching page, found page — one is shown at a time.
    enum class Page { Idle, Searching, Found };
    void showPage(Page page);

    QComboBox *m_band;
    QSpinBox *m_threshold;
    QStackedWidget *m_pages;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QPushButton *m_resetButton;
    SearchAnimation *m_animation;
    FoundDisplay *m_found;
};

} // namespace duality
