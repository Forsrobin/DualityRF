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

bool PeakDetector::signalPresent(const QVector<float> &db, double bandCenterHz,
                                 double halfWidthHz) const
{
    return signalPresent(db, bandCenterHz, halfWidthHz, m_thresholdDb);
}

bool PeakDetector::signalPresent(const QVector<float> &db, double bandCenterHz,
                                 double halfWidthHz, float thresholdDb) const
{
    const int n = db.size();
    if (n < 2 || m_spanHz <= 0.0)
        return false;

    int lo = 0;
    int hi = n - 1;
    if (halfWidthHz > 0.0) {
        const auto freqToBin = [&](double hz) {
            const double frac = 0.5 + (hz - m_centerHz) / m_spanHz;
            return static_cast<int>(std::lround(frac * (n - 1)));
        };
        lo = std::clamp(freqToBin(bandCenterHz - halfWidthHz), 0, n - 1);
        hi = std::clamp(freqToBin(bandCenterHz + halfWidthHz), 0, n - 1);
        if (lo > hi)
            std::swap(lo, hi);
    }
    for (int i = lo; i <= hi; ++i)
        if (db[i] > thresholdDb)
            return true;
    return false;
}

QVector<DetectedPeak> PeakDetector::update(const QVector<float> &db,
                                           QVector<DetectedPeak> *appeared)
{
    const int n = db.size();
    if (n < 2 || m_spanHz <= 0.0)
        return current();

    const double binHz = m_spanHz / (n - 1);
    // Bridge short sub-threshold dips so one modulated/fragmented transmission
    // (its carrier plus sideband bursts) stays a single detection, while two
    // genuinely separated in-band signals are still reported apart. Scaled to
    // the bin count so the bridged width is a fixed fraction of the span, with
    // a small floor for narrow FFTs.
    const int maxGapBins =
        std::max(2, static_cast<int>(std::lround(0.004 * (n - 1))));

    // Walk the spectrum grouping runs of above-threshold bins (bridging gaps up
    // to maxGapBins) into segments; each segment is one signal, reported at its
    // power-weighted center. Unlike a single whole-span centroid this does not
    // smear two concurrent signals into a phantom marker between them.
    QVector<DetectedPeak> found;
    int i = 0;
    while (i < n) {
        if (db[i] <= m_thresholdDb) {
            ++i;
            continue;
        }
        double weightSum = 0.0;
        double centroid = 0.0;
        float maxDb = m_thresholdDb;
        int lastAbove = i;
        int gap = 0;
        int j = i;
        for (; j < n; ++j) {
            if (db[j] > m_thresholdDb) {
                const double w = std::pow(10.0, db[j] / 10.0);
                weightSum += w;
                centroid += w * j;
                maxDb = std::max(maxDb, db[j]);
                lastAbove = j;
                gap = 0;
            } else if (++gap > maxGapBins) {
                break;
            }
        }
        DetectedPeak seg;
        seg.frequencyHz =
            m_centerHz + (centroid / weightSum / (n - 1) - 0.5) * m_spanHz;
        seg.powerDb = maxDb;
        found.push_back(seg);
        i = lastAbove + 1;
    }
    if (found.isEmpty())
        return current();

    // Merge each segment with an existing marker when its center lands close to
    // one, so a persistent signal keeps a single stable marker across frames.
    const double matchHz = std::max(4.0 * binHz, m_spanHz * 0.005);
    for (const DetectedPeak &seg : found) {
        DetectedPeak *best = nullptr;
        double bestDist = matchHz;
        for (DetectedPeak &t : m_tracks) {
            const double dist = std::abs(t.frequencyHz - seg.frequencyHz);
            if (dist <= bestDist) {
                bestDist = dist;
                best = &t;
            }
        }
        if (best) {
            best->frequencyHz = 0.7 * best->frequencyHz + 0.3 * seg.frequencyHz;
            best->powerDb = std::max(best->powerDb, seg.powerDb);
        } else {
            m_tracks.push_back(seg);
            if (appeared)
                appeared->push_back(seg);
        }
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
