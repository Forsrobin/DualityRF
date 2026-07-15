#include "dsp/Measurements.h"

#include "dsp/SpectrumAccumulator.h"

#include <algorithm>
#include <numeric>

namespace duality {

SpectrumStats analyzeSpectrum(const QVector<double> &linearShifted,
                              double sampleRateHz, double powerFraction)
{
    SpectrumStats stats;
    const int n = linearShifted.size();
    if (n == 0)
        return stats;

    const double binHz = sampleRateHz / n;
    const double total =
        std::accumulate(linearShifted.begin(), linearShifted.end(), 0.0);

    const auto peakIt =
        std::max_element(linearShifted.begin(), linearShifted.end());
    const int peakIdx = static_cast<int>(peakIt - linearShifted.begin());
    stats.peakDb = powerToDb(*peakIt);
    stats.meanDb = powerToDb(total / n);
    stats.peakOffsetHz = (peakIdx - n / 2) * binHz;

    if (total <= 0.0)
        return stats;

    // Occupied bandwidth: discard (1-fraction)/2 of total power from each
    // edge; what remains between the cut points is the OBW.
    const double edgePower = total * (1.0 - powerFraction) / 2.0;
    double acc = 0.0;
    int lo = 0;
    for (; lo < n; ++lo) {
        acc += linearShifted[lo];
        if (acc >= edgePower)
            break;
    }
    acc = 0.0;
    int hi = n - 1;
    for (; hi >= 0; --hi) {
        acc += linearShifted[hi];
        if (acc >= edgePower)
            break;
    }
    if (hi > lo)
        stats.occupiedBandwidthHz = (hi - lo + 1) * binHz;
    return stats;
}

} // namespace duality
