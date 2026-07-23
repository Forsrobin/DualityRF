#pragma once

#include <QWidget>

namespace duality {

class CaptureControlPanel;
class SessionList;
class SessionStore;
class SpectrumWidget;
class WaterfallWidget;

// The standalone "capture" program: a stripped-down FOB with just a CAPTURE
// control tab and the shared SESSIONS browser over the same switchable
// waterfall/FFT view — no concurrent TX, playback or replay. MainWindow reaches
// the panels and views through the accessors to wire them to the one shared
// capture pipeline.
class CaptureProgram : public QWidget {
    Q_OBJECT
public:
    explicit CaptureProgram(SessionStore *store, QWidget *parent = nullptr);

    CaptureControlPanel *capturePanel() const { return m_capturePanel; }
    SessionList *sessionList() const { return m_sessionList; }
    SpectrumWidget *spectrumView() const { return m_spectrumView; }
    WaterfallWidget *waterfallView() const { return m_waterfallView; }

private:
    CaptureControlPanel *m_capturePanel;
    SessionList *m_sessionList;
    SpectrumWidget *m_spectrumView;
    WaterfallWidget *m_waterfallView;
};

} // namespace duality
