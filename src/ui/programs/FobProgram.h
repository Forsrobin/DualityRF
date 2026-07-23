#pragma once

#include <QWidget>

namespace duality {

class CapturePanel;
class SessionList;
class SessionStore;
class SpectrumWidget;
class TransmitPanel;
class WaterfallWidget;

// The main "fob" program: one wrapping tab row of control panels (capture,
// concurrent TX, sessions) over a switchable waterfall/FFT visualization of the
// live capture. It owns the panels and views; MainWindow reaches them through
// the accessors to wire up the pipelines. Playback lives in the standalone
// Sessions program; device selection in the Devicees program.
class FobProgram : public QWidget {
    Q_OBJECT
public:
    explicit FobProgram(SessionStore *store, QWidget *parent = nullptr);

    CapturePanel *capturePanel() const { return m_capturePanel; }
    TransmitPanel *transmitPanel() const { return m_transmitPanel; }
    SessionList *sessionList() const { return m_sessionList; }
    SpectrumWidget *spectrumView() const { return m_spectrumView; }
    WaterfallWidget *waterfallView() const { return m_waterfallView; }

private:
    CapturePanel *m_capturePanel;
    TransmitPanel *m_transmitPanel;
    SessionList *m_sessionList;
    SpectrumWidget *m_spectrumView;
    WaterfallWidget *m_waterfallView;
};

} // namespace duality
