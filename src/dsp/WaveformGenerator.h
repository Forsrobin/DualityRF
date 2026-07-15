#pragma once

#include "core/Types.h"

#include <QString>

#include <memory>
#include <span>
#include <vector>

namespace duality {

enum class WaveformType {
    WhiteNoise,
    GaussianNoise,
    ContinuousWave,
    Sine,   // complex tone at a configurable offset from the carrier
    IqFile, // user-supplied IQ samples, looped
};

struct WaveformConfig {
    WaveformType type = WaveformType::ContinuousWave;
    double amplitude = 0.5; // 0..1 full scale
    // Frequency shift from the carrier, applied to every generator type:
    // the emitted signal is centered at carrier + offsetHz.
    double offsetHz = 100e3;
    // Bandwidth of the noise generators around that center (0 = full band).
    double rangeHz = 0.0;
    QString filePath; // IqFile only (decoded by the caller)

    QString describe() const;
};

// Produces successive chunks of baseband samples for the transmit pipeline.
// Implementations are stateful (phase, RNG, filters) and single-threaded.
class IWaveformGenerator {
public:
    virtual ~IWaveformGenerator() = default;
    virtual void generate(std::span<Complex> dst) = 0;
};

// `fileSamples` supplies the decoded IQ data for WaveformType::IqFile (the
// dsp layer stays independent of the storage layer).
std::unique_ptr<IWaveformGenerator>
makeWaveformGenerator(const WaveformConfig &config, double sampleRateHz,
                      std::vector<Complex> fileSamples = {});

const char *waveformName(WaveformType type);

} // namespace duality
