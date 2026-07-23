#pragma once

#include "core/Types.h"

#include <QString>

namespace duality {

// Read-only view of a capture control panel that MainWindow drives, so the FOB
// and CAPTURE programs can both feed the one shared capture pipeline. Both
// CapturePanel (FOB) and CaptureControlPanel (CAPTURE) implement it.
class ICaptureControls {
public:
    virtual ~ICaptureControls() = default;

    virtual StreamParams streamParams() const = 0;
    // "manual" or "auto".
    virtual QString trigger() const = 0;
    virtual double peakThresholdDb() const = 0;
    virtual double captureRangeHz() const = 0;
    virtual double hangTimeMs() const = 0;
    // Replay mode is a FOB-only feature; the CAPTURE program returns false.
    virtual bool replayMode() const = 0;

    // Whether the live monitor should run peak detection. Defaults on; the
    // LOOKING program exposes it as a checkbox.
    virtual bool peakDetectionEnabled() const { return true; }

    virtual void setRunning(bool running) = 0;
};

} // namespace duality
