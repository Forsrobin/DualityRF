#pragma once

#include "dsp/PeakDetector.h"
#include "dsp/SpectrumProcessor.h"
#include "pipeline/CapturePipeline.h"
#include "pipeline/PlaybackPipeline.h"
#include "pipeline/TransmitPipeline.h"
#include "sdr/DeviceManager.h"
#include "storage/SessionStore.h"

#include <QMainWindow>

#include <chrono>
#include <memory>

class QStackedWidget;

namespace duality {

class AboutPage;
class CapturePanel;
class DebugWorkspace;
class DevicePanel;
class HomePage;
class PlaybackPanel;
class SessionBrowser;
class SplashPage;
class SpectrumWidget;
class ToastManager;
class TransmitPanel;
class VizPanel;
class WaterfallWidget;

// Owns the application services (device manager, session store, pipelines)
// and wires the dockable panels to them. Views: live capture (spectrum +
// waterfall) and the offline debug workspace.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startCapture(bool record);
    void stopCapture();
    void onCaptureFinished(const duality::RecordingMetadata &meta,
                           bool wasRecording);
    void startPlayback(const QString &absPath, const StreamParams &params,
                       SampleFormat format, double speed, bool repeat);

private:
    // Builds the Fob page: one tab row of control panels (top 3/4) over the
    // switchable waterfall/FFT visualization (bottom 1/4).
    QWidget *buildFobPage();
    // Registers a program: adds a tile to the home grid and pushes `content`
    // (wrapped in a back-bar) onto the stack. Returns the wrapped screen.
    QWidget *addProgram(const QString &name, const QString &iconPath,
                        QWidget *content);
    Session *ensureSession();
    void cacheRecordingSpectrum(const RecordingMetadata &meta);
    void updateCaptureRangeOverlay();
    // Opens the transmitter and starts the concurrent waveform on the given
    // capture channel; returns false if it could not start.
    bool startConcurrentTx(const StreamParams &captureParams);
    // Toggles the concurrent transmission on/off while a capture is running.
    void onTransmitToggled(bool enabled);

    // Auto-capture: continuously records each in-band transmission to its own
    // trimmed file.
    enum class AutoPhase { WaitSignal, Recording, Finalizing };
    void startAutoCapture();
    void driveAutoCapture(const QVector<float> &row);
    void onAutoSegmentSaved(const RecordingMetadata &meta);
    // Re-arm for the next transmission after a segment was saved or dropped.
    void rearmAuto();
    // Replay mode: after two auto segments, stop the concurrent TX noise, drop
    // out of segment capture into a plain monitor, and replay the first signal.
    void enterReplayMonitor();

    // Services
    DeviceManager m_deviceManager;
    SessionStore m_store;
    std::unique_ptr<Session> m_session;
    SpectrumProcessor m_spectrumProcessor;
    PeakDetector m_peakDetector;
    StreamParams m_activeCaptureParams; // channel of the running capture
    bool m_monitorLive = false;
    bool m_autoActive = false;
    AutoPhase m_autoPhase = AutoPhase::WaitSignal;
    double m_autoBandCenterHz = 0.0;
    double m_autoBandHalfHz = 0.0;
    double m_autoSampleRateHz = 0.0;
    // Separate on/off levels give the presence test hysteresis so an envelope
    // that hovers near the threshold does not chatter the state machine.
    float m_autoOnThresholdDb = 0.0f;
    float m_autoOffThresholdDb = 0.0f;
    // Time the segment stays open after the signal drops (bridges modulation
    // gaps). Set from the panel when auto-capture starts.
    std::chrono::milliseconds m_autoHangTime{500};
    std::chrono::steady_clock::time_point m_autoLastSignal;
    static constexpr float kAutoHysteresisDb = 6.0f;
    static constexpr auto kAutoPreRoll = std::chrono::milliseconds(100);
    static constexpr auto kAutoPostPad = std::chrono::milliseconds(100);
    static constexpr auto kAutoMinSegment = std::chrono::milliseconds(150);

    // Replay-mode session state (armed by the panel's REPLAY MODE button).
    bool m_replayActive = false;
    int m_replaySegmentCount = 0;
    RecordingMetadata m_replayFirstMeta;
    QString m_replayFirstPath;
    CapturePipeline m_capture;
    PlaybackPipeline m_playback;
    TransmitPipeline m_transmit;

    // Views
    QStackedWidget *m_stack;
    SplashPage *m_splash;
    HomePage *m_home;
    QStackedWidget *m_panelStack;
    SpectrumWidget *m_spectrumView;
    WaterfallWidget *m_waterfallView;
    VizPanel *m_viz;
    DebugWorkspace *m_debugWorkspace;
    QWidget *m_debugScreen = nullptr; // back-bar wrapper around the workspace
    AboutPage *m_aboutPage;
    ToastManager *m_toasts;

    static constexpr int kWindowWidth = 320;
    static constexpr int kWindowHeight = 480;

    // Panels
    DevicePanel *m_devicePanel;
    CapturePanel *m_capturePanel;
    TransmitPanel *m_transmitPanel;
    PlaybackPanel *m_playbackPanel;
    SessionBrowser *m_sessionBrowser;
};

} // namespace duality
