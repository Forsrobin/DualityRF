#pragma once

#include "dsp/PeakDetector.h"
#include "dsp/SpectrumProcessor.h"
#include "pipeline/CapturePipeline.h"
#include "pipeline/PlaybackPipeline.h"
#include "pipeline/TransmitPipeline.h"
#include "sdr/DeviceManager.h"
#include "storage/SessionStore.h"

#include <QMainWindow>
#include <QVector>

#include <chrono>
#include <memory>

class QStackedWidget;

namespace duality {

class CaptureControlPanel;
class CaptureProgram;
class CapturePanel;
class DebugProgram;
class ICaptureControls;
class DevicesProgram;
class FobProgram;
class HomePage;
class InfoProgram;
class LookingPanel;
class LookingProgram;
class PlaybackPanel;
class ProgramScreen;
class SessionList;
class SessionsProgram;
class SplashPage;
class SpectrumWidget;
class ToastManager;
class TransmitPanel;
class JamProgram;
class WaterfallWidget;

// Owns the application services (device manager, session store, pipelines)
// and hosts the programs (FOB, Devicees, Jam, Debug, About) on a home-grid
// launcher, wiring their panels to the pipelines. The Devicees program is the
// app-wide device selector; each program's top bar shows and links to it.
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
    ProgramScreen *addProgram(const QString &name, const QString &iconPath,
                              QWidget *content);

    // Which device section the Devicees program should reveal when opened.
    enum class DeviceFocus { None, Rx, Tx };
    // Switch to the Devicees program, remembering the current screen so its
    // back arrow returns here. No-op if already on it.
    void openDevices(DeviceFocus focus);
    // Switch to the Sessions manager, remembering the current screen for its
    // back arrow. An empty dir opens the list; otherwise it jumps to that
    // session's detail page.
    void openSessions(const QString &sessionDir);
    // Push the current RX/TX selection into every program's top-bar badges.
    void refreshDeviceBadges();

    Session *ensureSession();
    // Point the shared capture pipeline at whichever program's controls just
    // armed it; allowTx gates the FOB-only concurrent TX + replay features.
    void setActiveCapture(ICaptureControls *controls, bool allowTx);
    // Refresh every session browser (FOB + CAPTURE) after the store changes.
    void refreshBrowsers();
    void cacheRecordingSpectrum(const RecordingMetadata &meta);
    void updateCaptureRangeOverlay();
    // Opens the transmitter and starts the concurrent waveform on the given
    // capture channel; returns false if it could not start.
    bool startConcurrentTx(const StreamParams &captureParams);
    // Toggles the concurrent transmission on/off while a capture is running.
    void onTransmitToggled(bool enabled);

    // Standalone Jam program: transmit the configured waveform and, when a
    // receiver is selected, monitor it live into the program's FFT/waterfall.
    void startJam();
    void stopJam();

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
    SpectrumProcessor m_jamProcessor; // live spectrum for the Jam program monitor
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

    // Replay-mode session state (armed by the panel's "Replay mode" checkbox).
    bool m_replayActive = false;
    // Whether the concurrent TX is dropped once the second segment is captured
    // (the panel's "Disable TX after capture" checkbox). Latched at start.
    bool m_disableTxAfterReplay = true;
    int m_replaySegmentCount = 0;
    RecordingMetadata m_replayFirstMeta;
    QString m_replayFirstPath;
    CapturePipeline m_capture;
    CapturePipeline m_jamMonitor; // RX monitor feeding the Jam program's viz
    PlaybackPipeline m_playback;
    TransmitPipeline m_transmit;

    // Views
    QStackedWidget *m_stack;
    SplashPage *m_splash;
    HomePage *m_home;
    FobProgram *m_fobProgram;
    // Live spectrum/waterfall views to broadcast each frame to — the FOB and
    // CAPTURE programs each own one pair (owned there, aliased here).
    QVector<SpectrumWidget *> m_spectrumViews;
    QVector<WaterfallWidget *> m_waterfallViews;
    DebugProgram *m_debugProgram;
    QWidget *m_debugScreen = nullptr; // back-bar wrapper around the workspace
    InfoProgram *m_infoProgram;
    JamProgram *m_jamProgram;
    QWidget *m_jamScreen = nullptr;    // back-bar wrapper around the Jam program
    bool m_jamActive = false;   // Jam program is transmitting
    // App-wide device selector; the pipelines read its selection.
    DevicesProgram *m_devicesProgram;
    int m_devicesIndex = -1;        // stack index of the Devicees screen
    int m_devicesReturnIndex = 0;   // where its back arrow returns to
    // Every program's back-bar, so device badges can be refreshed in one pass.
    QVector<ProgramScreen *> m_programScreens;
    ToastManager *m_toasts;

    static constexpr int kWindowWidth = 320;
    static constexpr int kWindowHeight = 480;

    // FOB panels — owned by m_fobProgram, aliased here for pipeline wiring.
    CapturePanel *m_capturePanel;
    TransmitPanel *m_transmitPanel;
    PlaybackPanel *m_playbackPanel;
    SessionList *m_fobSessionList; // owned by m_fobProgram

    // Standalone CAPTURE program: a simplified capture panel + its own session
    // list, both feeding the one shared capture pipeline.
    CaptureProgram *m_captureProgram = nullptr;
    SessionList *m_captureSessionList = nullptr; // owned by m_captureProgram

    // Full session manager, opened from the home grid or a session tile.
    SessionsProgram *m_sessionsProgram = nullptr;
    int m_sessionsIndex = -1;      // stack index of the Sessions screen
    int m_sessionsReturnIndex = 0; // where its back arrow returns to
    // Playback panel currently driving the shared pipeline (FOB or Sessions).
    PlaybackPanel *m_activePlayback = nullptr;
    // Controls currently driving the pipeline (FOB or CAPTURE panel) and whether
    // that source may use concurrent TX / replay (FOB only).
    ICaptureControls *m_activeCapture = nullptr;
    bool m_activeAllowTx = true;
    // Whether the live monitor runs peak detection. Always on for FOB/CAPTURE;
    // the LOOKING program toggles it. Set at capture start and live via the
    // panel's checkbox.
    bool m_peakMarkersEnabled = true;

    // Monitor-only LOOKING glass program.
    LookingProgram *m_lookingProgram = nullptr;
    LookingPanel *m_lookingPanel = nullptr; // owned by m_lookingProgram
};

} // namespace duality
