#include "dsp/PeakDetector.h"

#include <algorithm>
#include <cmath>

namespace duality {

namespace {

// Keep only the strongest markers if a long monitor session accumulates
// signals on many distinct frequencies.
constexpr int kMaxTracks = 8;

} // namespace

void PeakDetector::setAxis(double centerHz, double spanHz)
{
    m_centerHz = centerHz;
    m_spanHz = spanHz;
}

void PeakDetector::reset()
{
    m_tracks.clear();
}

QVector<DetectedPeak> PeakDetector::current() const
{
    QVector<DetectedPeak> result;
    result.reserve(static_cast<int>(m_tracks.size()));
    for (const DetectedPeak &p : m_tracks)
        result.push_back(p);
    return result;
}

QVector<DetectedPeak> PeakDetector::update(const QVector<float> &db,
                                           QVector<DetectedPeak> *appeared)
{
    const int n = db.size();
    if (n < 2 || m_spanHz <= 0.0)
        return current();

    // One detection per frame: the power-weighted middle of every bin above
    // the threshold. A transmission that crosses the threshold in several
    // fragments (modulation sidebands, noise bursts) still yields a single
    // center instead of one marker per fragment.
    double weightSum = 0.0;
    double centroid = 0.0;
    float maxDb = m_thresholdDb;
    bool any = false;
    for (int i = 0; i < n; ++i) {
        if (db[i] <= m_thresholdDb)
            continue;
        const double w = std::pow(10.0, db[i] / 10.0);
        weightSum += w;
        centroid += w * i;
        maxDb = std::max(maxDb, db[i]);
        any = true;
    }
    if (!any)
        return current();

    DetectedPeak found;
    found.frequencyHz =
        m_centerHz + (centroid / weightSum / (n - 1) - 0.5) * m_spanHz;
    found.powerDb = maxDb;

    // Merge with an existing marker when the center lands close to one, so
    // a persistent signal keeps a single stable marker across frames.
    const double binHz = m_spanHz / (n - 1);
    const double matchHz = std::max(4.0 * binHz, m_spanHz * 0.005);
    DetectedPeak *best = nullptr;
    double bestDist = matchHz;
    for (DetectedPeak &t : m_tracks) {
        const double dist = std::abs(t.frequencyHz - found.frequencyHz);
        if (dist <= bestDist) {
            bestDist = dist;
            best = &t;
        }
    }
    if (best) {
        best->frequencyHz =
            0.7 * best->frequencyHz + 0.3 * found.frequencyHz;
        best->powerDb = std::max(best->powerDb, found.powerDb);
    } else {
        m_tracks.push_back(found);
        if (appeared)
            appeared->push_back(found);
    }

    std::sort(m_tracks.begin(), m_tracks.end(),
              [](const DetectedPeak &a, const DetectedPeak &b) {
                  return a.powerDb > b.powerDb;
              });
    if (static_cast<int>(m_tracks.size()) > kMaxTracks)
        m_tracks.resize(kMaxTracks);
    return current();
}

} // namespace duality
