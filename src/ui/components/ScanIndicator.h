#pragma once

#include <QWidget>

class QTimer;

namespace duality {

// A monochrome "Cylon" sweep bar: a lit segment bounces back and forth with a
// fading trail while scanning, and settles to a dim static row when idle. Pure
// eye-candy for the device scan, tuned to the app's pixel aesthetic.
class ScanIndicator : public QWidget {
public:
    explicit ScanIndicator(QWidget *parent = nullptr);

    void setScanning(bool on);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer *m_timer;
    int m_frame = 0;
    bool m_scanning = false;

    static constexpr int kSegments = 18;
};

} // namespace duality
