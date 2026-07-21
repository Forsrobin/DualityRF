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

class CapturePanel;
class DebugProgram;
class DevicePanel;
class FobProgram;
class HomePage;
class InfoProgram;
class PlaybackPanel;
class SessionBrowser;
class SplashPage;
class SpectrumWidget;
class ToastManager;
class TransmitPanel;
class TxProgram;
class WaterfallWidget;

// Owns the application services (device manager, session store, pipelines)
// and hosts the programs (FOB, TX, Debug, Info) on a home-grid launcher,
// wiring their panels to the pipelines.
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

    // Standalone TX program: transmit the configured waveform and, when a
    // receiver is selected, monitor it live into the program's FFT/waterfall.
    void startTxProgram();
    void stopTxProgram();

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
    SpectrumProcessor m_txProcessor; // live spectrum for the TX program monitor
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
    CapturePipeline m_txMonitor; // RX monitor feeding the TX program's viz
    PlaybackPipeline m_playback;
    TransmitPipeline m_transmit;

    // Views
    QStackedWidget *m_stack;
    SplashPage *m_splash;
    HomePage *m_home;
    FobProgram *m_fobProgram;
    SpectrumWidget *m_spectrumView;   // owned by m_fobProgram (alias)
    WaterfallWidget *m_waterfallView; // owned by m_fobProgram (alias)
    DebugProgram *m_debugProgram;
    QWidget *m_debugScreen = nullptr; // back-bar wrapper around the workspace
    InfoProgram *m_infoProgram;
    TxProgram *m_txProgram;
    QWidget *m_txScreen = nullptr;    // back-bar wrapper around the TX program
    bool m_txProgramActive = false;   // TX program is transmitting
    ToastManager *m_toasts;

    static constexpr int kWindowWidth = 320;
    static constexpr int kWindowHeight = 480;

    // FOB panels — owned by m_fobProgram, aliased here for pipeline wiring.
    DevicePanel *m_devicePanel;
    CapturePanel *m_capturePanel;
    TransmitPanel *m_transmitPanel;
    PlaybackPanel *m_playbackPanel;
    SessionBrowser *m_sessionBrowser;
};

} // namespace duality
