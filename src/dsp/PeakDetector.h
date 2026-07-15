#pragma once

#include <QVector>

#include <vector>

namespace duality {

struct DetectedPeak {
    double frequencyHz = 0.0;
    float powerDb = 0.0f;
};

// Detects at most one signal per fftshifted spectrum trace: the
// power-weighted middle of every above-threshold bin, so a fragmented or
// modulated transmission yields a single marker at its center. Detections
// persist until reset() (i.e. until the monitor stage is stopped); a signal
// at a genuinely new frequency adds a second persistent marker. Each marker
// is reported exactly once when it first appears (for logging).
class PeakDetector {
public:
    void setAxis(double centerHz, double spanHz);
    void setThresholdDb(float thresholdDb) { m_thresholdDb = thresholdDb; }
    void reset();

    // Processes one trace and returns the persistent peaks, strongest
    // first. Peaks seen for the first time are appended to *appeared.
    QVector<DetectedPeak> update(const QVector<float> &db,
                                 QVector<DetectedPeak> *appeared = nullptr);

private:
    QVector<DetectedPeak> current() const;

    double m_centerHz = 0.0;
    double m_spanHz = 0.0;
    float m_thresholdDb = -25.0f;
    std::vector<DetectedPeak> m_tracks;
};

} // namespace duality
