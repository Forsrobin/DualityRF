#pragma once

#include "sdr/IReceiver.h"
#include "sdr/ISDRDevice.h"
#include "sdr/ITransmitter.h"

#include <memory>
#include <mutex>

namespace SoapySDR {
class Device;
class Stream;
}

namespace duality {

// The only place in the codebase that talks to SoapySDR for streaming and
// tuning. RX and TX are exposed through inner classes because IReceiver and
// ITransmitter deliberately share their method vocabulary.
class SoapyDevice final : public ISDRDevice {
public:
    // Returns nullptr (with a qWarning) when the driver refuses to open.
    static std::shared_ptr<SoapyDevice> open(const DeviceInfo &info);

    ~SoapyDevice() override;
    SoapyDevice(const SoapyDevice &) = delete;
    SoapyDevice &operator=(const SoapyDevice &) = delete;

    const DeviceInfo &info() const override { return m_info; }
    IReceiver *receiver() override;
    ITransmitter *transmitter() override;
    Range sampleRateRange() const override;
    Range frequencyRange() const override;

private:
    SoapyDevice(SoapySDR::Device *dev, DeviceInfo info);

    class RxStream;
    class TxStream;

    SoapySDR::Device *m_dev = nullptr;
    DeviceInfo m_info;
    mutable std::mutex m_mutex; // serializes tuning against the driver
    std::unique_ptr<RxStream> m_rx;
    std::unique_ptr<TxStream> m_tx;
};

} // namespace duality
