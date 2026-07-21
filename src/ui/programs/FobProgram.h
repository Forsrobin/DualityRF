#pragma once

#include <QWidget>

namespace duality {

class CapturePanel;
class DeviceManager;
class DevicePanel;
class PlaybackPanel;
class SessionBrowser;
class SessionStore;
class SpectrumWidget;
class TransmitPanel;
class WaterfallWidget;

// The main "fob" program: one wrapping tab row of control panels (devices,
// capture, concurrent TX, sessions, playback) over a switchable waterfall/FFT
// visualization of the live capture. It owns the panels and views; MainWindow
// reaches them through the accessors to wire up the pipelines.
class FobProgram : public QWidget {
    Q_OBJECT
public:
    FobProgram(DeviceManager *deviceManager, SessionStore *store,
               QWidget *parent = nullptr);

    DevicePanel *devicePanel() const { return m_devicePanel; }
    CapturePanel *capturePanel() const { return m_capturePanel; }
    TransmitPanel *transmitPanel() const { return m_transmitPanel; }
    PlaybackPanel *playbackPanel() const { return m_playbackPanel; }
    SessionBrowser *sessionBrowser() const { return m_sessionBrowser; }
    SpectrumWidget *spectrumView() const { return m_spectrumView; }
    WaterfallWidget *waterfallView() const { return m_waterfallView; }

private:
    DevicePanel *m_devicePanel;
    CapturePanel *m_capturePanel;
    TransmitPanel *m_transmitPanel;
    PlaybackPanel *m_playbackPanel;
    SessionBrowser *m_sessionBrowser;
    SpectrumWidget *m_spectrumView;
    WaterfallWidget *m_waterfallView;
};

} // namespace duality
