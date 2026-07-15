#pragma once

#include "core/Types.h"

#include <span>

namespace duality {

class IReceiver {
public:
    virtual ~IReceiver() = default;

    virtual bool configure(const StreamParams &params) = 0;
    virtual bool start() = 0;

    // Blocks up to timeoutUs; returns the number of samples written to dst
    // (0 on timeout, so callers can poll a stop token between reads).
    virtual std::size_t read(std::span<Complex> dst, long timeoutUs = 100000) = 0;

    virtual void stop() = 0;
};

} // namespace duality
