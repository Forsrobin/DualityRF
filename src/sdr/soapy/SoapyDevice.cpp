#include "sdr/soapy/SoapyDevice.h"

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

#include <QDebug>

#include <algorithm>
#include <string>
#include <vector>

namespace duality {

namespace {

SoapySDR::Kwargs toKwargs(const QMap<QString, QString> &args)
{
    SoapySDR::Kwargs kwargs;
    for (auto it = args.constBegin(); it != args.constEnd(); ++it)
        kwargs[it.key().toStdString()] = it.value().toStdString();
    return kwargs;
}

Range overallRange(const SoapySDR::RangeList &ranges)
{
    Range r;
    if (ranges.empty())
        return r;
    r.min = ranges.front().minimum();
    r.max = ranges.front().maximum();
    for (const auto &sr : ranges) {
        r.min = std::min(r.min, sr.minimum());
        r.max = std::max(r.max, sr.maximum());
    }
    return r;
}

} // namespace

// Common stream logic for one direction; direction-specific behavior is just
// the SoapySDR direction constant and read vs write.
class SoapyStreamBase {
public:
    SoapyStreamBase(SoapySDR::Device *dev, std::mutex &mutex, int direction)
        : m_dev(dev), m_mutex(mutex), m_direction(direction)
    {
    }

    ~SoapyStreamBase() { stopStream(); }

    bool configureStream(const StreamParams &p)
    {
        std::scoped_lock lock(m_mutex);
        try {
            m_dev->setSampleRate(m_direction, 0, p.sampleRateHz);
            m_dev->setFrequency(m_direction, 0, p.frequencyHz);
            if (p.bandwidthHz > 0.0)
                m_dev->setBandwidth(m_direction, 0, p.bandwidthHz);
            applyGain(p.gainDb);
            return true;
        } catch (const std::exception &e) {
            qWarning() << "SoapyDevice: configure failed:" << e.what();
            return false;
        }
    }

    // Set the requested gain so it matches what the radio applies. On TX,
    // SoapySDR's aggregate setGain() splits the value across every stage,
    // e.g. a HackRF puts 14 dB into the fixed RF amp and the rest into the
    // variable VGA — so a requested 40 dB shows up as VGA 26 dB. Instead put
    // the whole requested gain into the variable stage (the widest-range gain
    // element) and leave any fixed amp off, so 40 in means VGA 40. RX keeps
    // the aggregate behaviour (max sensitivity across its stages).
    void applyGain(double gainDb)
    {
        if (m_direction != SOAPY_SDR_TX) {
            m_dev->setGain(m_direction, 0, gainDb);
            return;
        }
        const std::vector<std::string> names = m_dev->listGains(m_direction, 0);
        if (names.size() <= 1) {
            m_dev->setGain(m_direction, 0, gainDb);
            return;
        }
        // The widest-range element is the variable gain; the rest are amps.
        std::string variable;
        double widest = -1.0;
        for (const std::string &name : names) {
            const SoapySDR::Range r =
                m_dev->getGainRange(m_direction, 0, name);
            const double span = r.maximum() - r.minimum();
            if (span > widest) {
                widest = span;
                variable = name;
            }
        }
        for (const std::string &name : names) {
            const SoapySDR::Range r =
                m_dev->getGainRange(m_direction, 0, name);
            const double value =
                name == variable
                    ? std::clamp(gainDb, r.minimum(), r.maximum())
                    : r.minimum(); // amp off
            m_dev->setGain(m_direction, 0, name, value);
        }
    }

    bool startStream()
    {
        std::scoped_lock lock(m_mutex);
        if (m_stream)
            return true;
        try {
            m_stream = m_dev->setupStream(m_direction, SOAPY_SDR_CF32);
            m_dev->activateStream(m_stream);
            return true;
        } catch (const std::exception &e) {
            qWarning() << "SoapyDevice: stream start failed:" << e.what();
            if (m_stream) {
                m_dev->closeStream(m_stream);
                m_stream = nullptr;
            }
            return false;
        }
    }

    void stopStream()
    {
        std::scoped_lock lock(m_mutex);
        if (!m_stream)
            return;
        m_dev->deactivateStream(m_stream);
        m_dev->closeStream(m_stream);
        m_stream = nullptr;
    }

protected:
    SoapySDR::Device *m_dev;
    std::mutex &m_mutex;
    const int m_direction;
    SoapySDR::Stream *m_stream = nullptr;
};

class SoapyDevice::RxStream final : public IReceiver, public SoapyStreamBase {
public:
    RxStream(SoapySDR::Device *dev, std::mutex &mutex)
        : SoapyStreamBase(dev, mutex, SOAPY_SDR_RX)
    {
    }

    bool configure(const StreamParams &p) override { return configureStream(p); }
    bool start() override { return startStream(); }
    void stop() override { stopStream(); }

    std::size_t read(std::span<Complex> dst, long timeoutUs) override
    {
        if (!m_stream)
            return 0;
        void *buffs[] = {dst.data()};
        int flags = 0;
        long long timeNs = 0;
        const int ret = m_dev->readStream(m_stream, buffs, dst.size(), flags,
                                          timeNs, timeoutUs);
        if (ret == SOAPY_SDR_TIMEOUT || ret == SOAPY_SDR_OVERFLOW)
            return 0;
        if (ret < 0) {
            qWarning() << "SoapyDevice: readStream error" << ret;
            return 0;
        }
        return static_cast<std::size_t>(ret);
    }
};

class SoapyDevice::TxStream final : public ITransmitter, public SoapyStreamBase {
public:
    TxStream(SoapySDR::Device *dev, std::mutex &mutex)
        : SoapyStreamBase(dev, mutex, SOAPY_SDR_TX)
    {
    }

    bool configure(const StreamParams &p) override { return configureStream(p); }
    bool start() override { return startStream(); }
    void stop() override { stopStream(); }

    std::size_t write(std::span<const Complex> src, long timeoutUs) override
    {
        if (!m_stream)
            return 0;
        const void *buffs[] = {src.data()};
        int flags = 0;
        const int ret = m_dev->writeStream(m_stream, buffs, src.size(), flags,
                                           0, timeoutUs);
        if (ret == SOAPY_SDR_TIMEOUT || ret == SOAPY_SDR_UNDERFLOW)
            return 0;
        if (ret < 0) {
            qWarning() << "SoapyDevice: writeStream error" << ret;
            return 0;
        }
        return static_cast<std::size_t>(ret);
    }
};

std::shared_ptr<SoapyDevice> SoapyDevice::open(const DeviceInfo &info)
{
    try {
        SoapySDR::Device *dev = SoapySDR::Device::make(toKwargs(info.args));
        if (!dev)
            return nullptr;
        return std::shared_ptr<SoapyDevice>(new SoapyDevice(dev, info));
    } catch (const std::exception &e) {
        qWarning() << "SoapyDevice: open failed for" << info.displayName()
                   << ":" << e.what();
        return nullptr;
    }
}

SoapyDevice::SoapyDevice(SoapySDR::Device *dev, DeviceInfo info)
    : m_dev(dev), m_info(std::move(info))
{
    if (m_info.canReceive)
        m_rx = std::make_unique<RxStream>(m_dev, m_mutex);
    if (m_info.canTransmit)
        m_tx = std::make_unique<TxStream>(m_dev, m_mutex);
}

SoapyDevice::~SoapyDevice()
{
    m_rx.reset();
    m_tx.reset();
    if (m_dev)
        SoapySDR::Device::unmake(m_dev);
}

IReceiver *SoapyDevice::receiver()
{
    return m_rx.get();
}

ITransmitter *SoapyDevice::transmitter()
{
    return m_tx.get();
}

Range SoapyDevice::sampleRateRange() const
{
    std::scoped_lock lock(m_mutex);
    const int dir = m_info.canReceive ? SOAPY_SDR_RX : SOAPY_SDR_TX;
    try {
        return overallRange(m_dev->getSampleRateRange(dir, 0));
    } catch (const std::exception &) {
        return {};
    }
}

Range SoapyDevice::frequencyRange() const
{
    std::scoped_lock lock(m_mutex);
    const int dir = m_info.canReceive ? SOAPY_SDR_RX : SOAPY_SDR_TX;
    try {
        return overallRange(m_dev->getFrequencyRange(dir, 0));
    } catch (const std::exception &) {
        return {};
    }
}

} // namespace duality
