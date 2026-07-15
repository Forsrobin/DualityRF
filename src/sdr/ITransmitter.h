#pragma once

#include "core/Types.h"

#include <span>

namespace duality {

class ITransmitter {
public:
    virtual ~ITransmitter() = default;

    virtual bool configure(const StreamParams &params) = 0;
    virtual bool start() = 0;

    // Returns the number of samples consumed from src (may be short when the
    // hardware buffer is full).
    virtual std::size_t write(std::span<const Complex> src, long timeoutUs = 100000) = 0;

    virtual void stop() = 0;
};

} // namespace duality
