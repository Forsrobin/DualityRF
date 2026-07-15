#pragma once

#include <QVector>
#include <QWidget>

namespace duality {

// A labelled vertical line pinned to a frequency (detected signal center).
struct SpectrumMarker {
    double frequencyHz = 0.0;
    float powerDb = 0.0f;
};

// FFT trace display with horizontal zoom (wheel), pan (drag) and optional
// peak hold. Supports a secondary trace for A/B comparison in the debug
// workspace, detected-peak markers and a shaded capture-range band. Trace
// colors stay grayscale; the colormap belongs to the waterfall.
class SpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

    void setAxis(double centerHz, double spanHz);
    void setTrace(const QVector<float> &db);
    void setSecondaryTrace(const QVector<float> &db);
    void setPeakHold(bool enabled);
    void setMarkers(const QVector<SpectrumMarker> &markers);
    // Shaded band showing what a recording will capture; widthHz <= 0 hides
    // it.
    void setCaptureRange(double centerHz, double widthHz);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    // Visible window in normalized spectrum coordinates [0,1].
    double visibleStart() const { return m_viewCenter - 0.5 / m_zoom; }
    double visibleWidth() const { return 1.0 / m_zoom; }
    void clampView();
    // Pixel x-position of an absolute frequency in the current view, or -1
    // if it falls outside the visible window.
    int frequencyToX(double hz) const;
    void drawTrace(QPainter &painter, const QVector<float> &db,
                   const QColor &color) const;
    void drawCaptureRange(QPainter &painter) const;
    void drawMarkers(QPainter &painter) const;

    QVector<float> m_trace;
    QVector<float> m_secondary;
    QVector<float> m_peaks;
    QVector<SpectrumMarker> m_markers;
    bool m_peakHold = false;

    double m_rangeCenterHz = 0.0;
    double m_rangeWidthHz = 0.0;

    double m_centerHz = 0.0;
    double m_spanHz = 0.0;
    double m_zoom = 1.0;       // 1 .. 64
    double m_viewCenter = 0.5; // normalized
    int m_dragX = -1;
};

} // namespace duality
