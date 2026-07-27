#include "dsp/WaveformGenerator.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <random>
#include <string>

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

// International Morse for the characters a beacon message needs: the letters,
// digits and the handful of punctuation marks used in callsigns and CQ calls.
// '.' is a dit (1 unit), '-' a dah (3 units); anything not listed is skipped.
const char *morseCode(char c)
{
    switch (c) {
    case 'A': return ".-";    case 'B': return "-...";  case 'C': return "-.-.";
    case 'D': return "-..";   case 'E': return ".";     case 'F': return "..-.";
    case 'G': return "--.";   case 'H': return "....";  case 'I': return "..";
    case 'J': return ".---";  case 'K': return "-.-";   case 'L': return ".-..";
    case 'M': return "--";    case 'N': return "-.";    case 'O': return "---";
    case 'P': return ".--.";  case 'Q': return "--.-";  case 'R': return ".-.";
    case 'S': return "...";   case 'T': return "-";     case 'U': return "..-";
    case 'V': return "...-";  case 'W': return ".--";   case 'X': return "-..-";
    case 'Y': return "-.--";  case 'Z': return "--..";
    case '0': return "-----"; case '1': return ".----"; case '2': return "..---";
    case '3': return "...--"; case '4': return "....-"; case '5': return ".....";
    case '6': return "-....";  case '7': return "--..."; case '8': return "---..";
    case '9': return "----.";
    case '.': return ".-.-.-"; case ',': return "--..--"; case '?': return "..--..";
    case '/': return "-..-.";  case '=': return "-...-";  case '+': return ".-.-.";
    case '-': return "-....-"; case '@': return ".--.-.";
    default: return "";
    }
}

// Keys a CW tone into International Morse. The message is compiled once into a
// schedule of key-on / key-off runs (in samples) at the PARIS timing derived
// from the WPM; generate() walks that schedule, looping forever so it works as
// a beacon. Each key-on run is shaped with a raised-cosine rise/fall so the
// transmitted envelope has no hard edges — that keeps the keyed spectrum tight
// and free of the "key clicks" a bare on/off gate would splatter either side.
class MorseGen final : public IWaveformGenerator {
public:
    MorseGen(float amplitude, double toneHz, double wpm, const QString &text,
             double sampleRateHz)
        : m_amplitude(amplitude), m_nco(toneHz, sampleRateHz)
    {
        // PARIS standard: one dit = 1.2 s / WPM. Guard against nonsense input.
        const double unit = 1.2 / std::max(wpm, 1.0);
        const auto units = [&](int n) {
            return std::max<std::size_t>(
                1, static_cast<std::size_t>(unit * n * sampleRateHz));
        };
        // Click-suppression ramp: ~4 ms, but never more than half a dit so
        // even fast keying still reaches full amplitude.
        m_ramp = static_cast<std::size_t>(0.004 * sampleRateHz);

        const std::string msg = text.toUpper().toStdString();
        bool prevWasChar = false;
        bool pendingWordGap = false;
        for (char c : msg) {
            if (c == ' ') {
                if (prevWasChar)
                    pendingWordGap = true;
                continue;
            }
            const char *code = morseCode(c);
            if (*code == '\0')
                continue;
            if (prevWasChar) // gap since the previous character
                m_segments.push_back({false, units(pendingWordGap ? 7 : 3)});
            pendingWordGap = false;
            for (int i = 0; code[i] != '\0'; ++i) {
                if (i > 0) // intra-character (element) gap
                    m_segments.push_back({false, units(1)});
                m_segments.push_back({true, units(code[i] == '-' ? 3 : 1)});
            }
            prevWasChar = true;
        }
        // Trailing word gap so the looping beacon breathes between repeats;
        // an empty/uncodable message becomes plain silence.
        m_segments.push_back({false, units(prevWasChar ? 7 : 1)});
    }

    void generate(std::span<Complex> dst) override
    {
        for (auto &s : dst) {
            const Segment &seg = m_segments[m_seg];
            // The tone runs continuously (phase stays coherent across dits);
            // the envelope gates and shapes it.
            const Complex tone = m_nco.next();
            float env = 0.0f;
            if (seg.keyOn) {
                const std::size_t r = std::min(m_ramp, seg.samples / 2);
                if (r == 0)
                    env = 1.0f;
                else if (m_pos < r)
                    env = raisedCosine(m_pos, r);
                else if (m_pos >= seg.samples - r)
                    env = raisedCosine(seg.samples - 1 - m_pos, r);
                else
                    env = 1.0f;
            }
            s = (m_amplitude * env) * tone;

            if (++m_pos >= seg.samples) {
                m_pos = 0;
                if (++m_seg >= m_segments.size())
                    m_seg = 0; // loop the beacon
            }
        }
    }

private:
    struct Segment {
        bool keyOn;
        std::size_t samples;
    };
    // Half-cosine rise from 0→1 over `r` samples (edge of a keyed element).
    static float raisedCosine(std::size_t pos, std::size_t r)
    {
        const double x = static_cast<double>(pos) / static_cast<double>(r);
        return static_cast<float>(0.5 - 0.5 * std::cos(std::numbers::pi * x));
    }

    float m_amplitude;
    Nco m_nco;
    std::size_t m_ramp = 0;
    std::vector<Segment> m_segments;
    std::size_t m_seg = 0;
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
    case WaveformType::Morse:
        return std::make_unique<MorseGen>(amp, config.offsetHz, config.wpm,
                                          config.text, sampleRateHz);
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
    case WaveformType::Morse: return "CW beacon (Morse)";
    }
    return "?";
}

QString morsePreview(const QString &text)
{
    QString out;
    bool prevWasChar = false;
    bool pendingWordGap = false;
    for (char c : text.toUpper().toStdString()) {
        if (c == ' ') {
            if (prevWasChar)
                pendingWordGap = true;
            continue;
        }
        const char *code = morseCode(c);
        if (*code == '\0')
            continue;
        if (prevWasChar)
            out += pendingWordGap ? QStringLiteral(" / ") : QStringLiteral(" ");
        pendingWordGap = false;
        out += QString::fromLatin1(code);
        prevWasChar = true;
    }
    return out;
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
    if (type == WaveformType::Morse)
        s += QStringLiteral(" %1 wpm \"%2\"").arg(wpm).arg(text);
    return s;
}

} // namespace duality
