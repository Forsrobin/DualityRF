#include "dsp/WaveformGenerator.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <random>

namespace duality {

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;

// Numerically controlled oscillator: successive samples of e^{j·2π·f·t}.
class Nco {
public:
    Nco(double freqHz, double sampleRateHz)
        : m_step(kTwoPi * freqHz / sampleRateHz)
    {
    }

    Complex next()
    {
        const Complex v(static_cast<float>(std::cos(m_phase)),
                        static_cast<float>(std::sin(m_phase)));
        m_phase += m_step;
        if (m_phase > std::numbers::pi)
            m_phase -= kTwoPi;
        else if (m_phase < -std::numbers::pi)
            m_phase += kTwoPi;
        return v;
    }

private:
    double m_step;
    double m_phase = 0.0;
};

// Windowed-sinc FIR lowpass, normalized to preserve *power* for a white
// input: band-limiting concentrates the generator's power into the passband
// instead of attenuating it, so 50 kHz of noise at 1 MS/s is loud, not
// buried 13 dB down. Processes whole blocks over a contiguous buffer so the
// inner dot product vectorizes (Raspberry Pi friendly).
class FirLowpass {
public:
    FirLowpass(double cutoffHz, double sampleRateHz)
    {
        constexpr int kTaps = 129;
        const double fc =
            std::clamp(cutoffHz / sampleRateHz, 1e-6, 0.499); // normalized
        m_taps.resize(kTaps);
        const int mid = kTaps / 2;
        double power = 0.0;
        for (int n = 0; n < kTaps; ++n) {
            const double x = n - mid;
            const double sinc =
                x == 0.0 ? 2.0 * fc : std::sin(kTwoPi * fc * x) / (std::numbers::pi * x);
            const double hann =
                0.5 - 0.5 * std::cos(kTwoPi * n / (kTaps - 1));
            const double t = sinc * hann;
            m_taps[n] = static_cast<float>(t);
            power += t * t;
        }
        const float norm = static_cast<float>(1.0 / std::sqrt(power));
        for (float &t : m_taps)
            t *= norm;
        m_history.assign(kTaps - 1, Complex{});
    }

    void processBlock(std::span<Complex> data)
    {
        const std::size_t hist = m_history.size();
        m_work.resize(hist + data.size());
        std::copy(m_history.begin(), m_history.end(), m_work.begin());
        std::copy(data.begin(), data.end(), m_work.begin() + hist);

        // y[n] = Σ_k h[k]·x[n−k]  ⇔  dot(reverse(h), x[n−T+1 .. n]); with
        // taps symmetric, reverse(h) == h.
        for (std::size_t n = 0; n < data.size(); ++n) {
            float re = 0.0f, im = 0.0f;
            const Complex *x = m_work.data() + n;
            for (std::size_t k = 0; k < m_taps.size(); ++k) {
                re += m_taps[k] * x[k].real();
                im += m_taps[k] * x[k].imag();
            }
            data[n] = {re, im};
        }
        std::copy(m_work.end() - hist, m_work.end(), m_history.begin());
    }

private:
    std::vector<float> m_taps;
    std::vector<Complex> m_history;
    std::vector<Complex> m_work;
};

class WhiteNoiseGen final : public IWaveformGenerator {
public:
    explicit WhiteNoiseGen(float amplitude)
        : m_dist(-amplitude, amplitude)
    {
    }
    void generate(std::span<Complex> dst) override
    {
        for (auto &s : dst)
            s = {m_dist(m_rng), m_dist(m_rng)};
    }

private:
    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_dist;
};

class GaussianNoiseGen final : public IWaveformGenerator {
public:
    explicit GaussianNoiseGen(float amplitude)
        : m_dist(0.0f, amplitude / 3.0f) // ±3σ ≈ requested amplitude
    {
    }
    void generate(std::span<Complex> dst) override
    {
        for (auto &s : dst)
            s = {std::clamp(m_dist(m_rng), -1.0f, 1.0f),
                 std::clamp(m_dist(m_rng), -1.0f, 1.0f)};
    }

private:
    std::mt19937 m_rng{std::random_device{}()};
    std::normal_distribution<float> m_dist;
};

// CW and Sine: a single complex tone at the configured offset. The complex
// exponential occupies only carrier + offset; any mirror at carrier − offset
// seen over the air is hardware IQ-imbalance image, not part of the signal.
class ToneGen final : public IWaveformGenerator {
public:
    ToneGen(float amplitude, double offsetHz, double sampleRateHz)
        : m_amplitude(amplitude), m_nco(offsetHz, sampleRateHz)
    {
    }
    void generate(std::span<Complex> dst) override
    {
        for (auto &s : dst)
            s = m_amplitude * m_nco.next();
    }

private:
    float m_amplitude;
    Nco m_nco;
};

class LoopedVectorGen final : public IWaveformGenerator {
public:
    LoopedVectorGen(std::vector<Complex> samples, float amplitude)
        : m_samples(std::move(samples)), m_amplitude(amplitude)
    {
        if (m_samples.empty())
            m_samples.push_back({0.0f, 0.0f});
    }
    void generate(std::span<Complex> dst) override
    {
        for (auto &s : dst) {
            s = m_samples[m_pos] * m_amplitude;
            m_pos = (m_pos + 1) % m_samples.size();
        }
    }

private:
    std::vector<Complex> m_samples;
    float m_amplitude;
    std::size_t m_pos = 0;
};

// Band-limits a source to `rangeHz` and shifts it to `offsetHz`, so noise
// occupies [carrier + offset − range/2, carrier + offset + range/2].
class ShapedGen final : public IWaveformGenerator {
public:
    ShapedGen(std::unique_ptr<IWaveformGenerator> source, double offsetHz,
              double rangeHz, double sampleRateHz)
        : m_source(std::move(source))
        , m_nco(offsetHz, sampleRateHz)
        , m_mix(offsetHz != 0.0)
    {
        if (rangeHz > 0.0 && rangeHz < sampleRateHz)
            m_lowpass.emplace(rangeHz / 2.0, sampleRateHz);
    }

    void generate(std::span<Complex> dst) override
    {
        m_source->generate(dst);
        if (m_lowpass) {
            m_lowpass->processBlock(dst);
            // Power normalization can push occasional peaks past full scale.
            for (auto &s : dst)
                s = {std::clamp(s.real(), -1.0f, 1.0f),
                     std::clamp(s.imag(), -1.0f, 1.0f)};
        }
        if (m_mix)
            for (auto &s : dst)
                s *= m_nco.next();
    }

private:
    std::unique_ptr<IWaveformGenerator> m_source;
    std::optional<FirLowpass> m_lowpass;
    Nco m_nco;
    bool m_mix;
};

} // namespace

std::unique_ptr<IWaveformGenerator>
makeWaveformGenerator(const WaveformConfig &config, double sampleRateHz,
                      std::vector<Complex> fileSamples)
{
    const float amp =
        static_cast<float>(std::clamp(config.amplitude, 0.0, 1.0));

    std::unique_ptr<IWaveformGenerator> base;
    double rangeHz = 0.0;
    switch (config.type) {
    case WaveformType::WhiteNoise:
        base = std::make_unique<WhiteNoiseGen>(amp);
        rangeHz = config.rangeHz;
        break;
    case WaveformType::GaussianNoise:
        base = std::make_unique<GaussianNoiseGen>(amp);
        rangeHz = config.rangeHz;
        break;
    case WaveformType::ContinuousWave:
    case WaveformType::Sine:
        return std::make_unique<ToneGen>(amp, config.offsetHz, sampleRateHz);
    case WaveformType::IqFile:
        base = std::make_unique<LoopedVectorGen>(std::move(fileSamples), amp);
        break;
    }
    if (!base)
        return nullptr;
    if (config.offsetHz == 0.0 && rangeHz <= 0.0)
        return base;
    return std::make_unique<ShapedGen>(std::move(base), config.offsetHz,
                                       rangeHz, sampleRateHz);
}

const char *waveformName(WaveformType type)
{
    switch (type) {
    case WaveformType::WhiteNoise: return "White noise";
    case WaveformType::GaussianNoise: return "Gaussian noise";
    case WaveformType::ContinuousWave: return "Continuous wave";
    case WaveformType::Sine: return "Sine (offset)";
    case WaveformType::IqFile: return "IQ file";
    }
    return "?";
}

QString WaveformConfig::describe() const
{
    QString s = waveformName(type);
    s += QStringLiteral(" a=%1").arg(amplitude);
    if (offsetHz != 0.0)
        s += QStringLiteral(" offset=%1 kHz").arg(offsetHz / 1e3);
    if (rangeHz > 0.0 &&
        (type == WaveformType::WhiteNoise ||
         type == WaveformType::GaussianNoise))
        s += QStringLiteral(" range=%1 kHz").arg(rangeHz / 1e3);
    if (type == WaveformType::IqFile)
        s += ' ' + filePath;
    return s;
}

} // namespace duality
