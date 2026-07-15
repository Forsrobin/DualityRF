#pragma once

#include "core/Types.h"
#include "sdr/DeviceInfo.h"

namespace duality {

class IReceiver;
class ITransmitter;

// One opened SDR. Exposes its directions as narrow interfaces so pipelines
// only ever hold the capability they actually use.
class ISDRDevice {
public:
    virtual ~ISDRDevice() = default;

    virtual const DeviceInfo &info() const = 0;

    // nullptr when the device lacks the capability.
    virtual IReceiver *receiver() = 0;
    virtual ITransmitter *transmitter() = 0;

    virtual Range sampleRateRange() const = 0;
    virtual Range frequencyRange() const = 0;
};

} // namespace duality
