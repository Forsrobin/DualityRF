#pragma once

#include "dsp/PeakDetector.h"
#include "dsp/SpectrumProcessor.h"
#include "pipeline/CapturePipeline.h"
#include "pipeline/PlaybackPipeline.h"
#include "pipeline/RepeaterPipeline.h"
#include "pipeline/TransmitPipeline.h"
#include "sdr/DeviceManager.h"
#include "storage/SessionStore.h"

#include <QMainWindow>
#include <QVector>

#include <chrono>
#include <memory>

class QStackedWidget;
class QTimer;

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
class BeaconProgram;
class SentryProgram;
class RepeaterProgram;
class IdentifierProgram;
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

    // Standalone Beacon program: key the configured message as a looping CW
    // Morse beacon and, when a receiver is selected, monitor it live into the
    // program's FFT/waterfall (same pattern as the Jam program).
    void startBeacon();
    void stopBeacon();

    // Standalone Sentry program: a reactive jammer. Arming starts an RX monitor;
    // each detection frame is tested against the watch window/threshold, and a
    // crossing fires a timed jamming burst on the transmitter (then a cooldown).
    void startSentry();
    void stopSentry();
    // Evaluate one live detection frame from the sentry monitor.
    void onSentrySpectrum(const QVector<float> &row);
    // Fire / end one jamming burst.
    void fireSentryBurst();
    void endSentryBurst();

    // Standalone Repeater program: a store-and-forward relay. Arming starts the
    // repeater pipeline (RX -> detect -> buffer -> retransmit at an offset).
    void startRepeater();
    void stopRepeater();

    // Standalone Identifier program: monitors the selected common band and,
    // rather than latching the first peak (which catches a key fob's initial
    // bounce/resonance), captures the whole transmission as a max-hold envelope
    // and, once the channel goes quiet, reports the strongest peak's centre.
    // START begins the scan; RESET re-arms; leaving the screen stops it.
    void startIdentifier();
    void stopIdentifier();
    // Evaluate one live detection frame from the identifier monitor: track the
    // onset/quiet of a transmission and accumulate its max-hold envelope.
    void onIdentifierSpectrum(const QVector<float> &row);
    // The channel has gone quiet: analyse the captured envelope and report the
    // strongest, clearest peak's centre frequency (or re-arm if nothing stuck).
    void finishIdentifierBurst();

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
    SpectrumProcessor m_beaconProcessor; // live spectrum for the Beacon monitor
    SpectrumProcessor m_sentryProcessor; // detection spectrum for the Sentry
    SpectrumProcessor m_repeaterProcessor; // live spectrum for the Repeater
    SpectrumProcessor m_identifierProcessor; // detection spectrum for Identifier
    PeakDetector m_peakDetector;
    PeakDetector m_repeaterPeakDetector; // peak markers for the Repeater viz
    PeakDetector m_identifierPeakDetector; // finds the band's centre frequency
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
    CapturePipeline m_beaconMonitor; // RX monitor feeding the Beacon's viz
    CapturePipeline m_sentryMonitor; // RX monitor driving the Sentry detection
    CapturePipeline m_identifierMonitor; // RX monitor driving the Identifier
    RepeaterPipeline m_repeaterPipeline; // store-and-forward relay
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
    BeaconProgram *m_beaconProgram = nullptr;
    QWidget *m_beaconScreen = nullptr; // back-bar wrapper around the Beacon
    bool m_beaconActive = false;       // Beacon program is transmitting
    SentryProgram *m_sentryProgram = nullptr;
    QWidget *m_sentryScreen = nullptr; // back-bar wrapper around the Sentry
    bool m_sentryArmed = false;        // Sentry is watching for a trigger
    // Reactive-jammer state machine: WatchSignal listens, Firing radiates a
    // burst, Cooldown holds off before re-arming. Detection is ignored outside
    // WatchSignal so the jammer never re-triggers on its own signal.
    enum class SentryPhase { WatchSignal, Firing, Cooldown };
    SentryPhase m_sentryPhase = SentryPhase::WatchSignal;
    int m_sentryTriggerCount = 0;
    StreamParams m_sentryParams;        // channel the sentry monitor/burst share
    double m_sentryWatchCenterHz = 0.0; // detection window, latched at ARM
    double m_sentryWatchHalfHz = 0.0;
    float m_sentryThresholdDb = 0.0f;
    std::shared_ptr<ISDRDevice> m_sentryTxDevice; // held open for fast bursts
    QTimer *m_sentryBurstTimer = nullptr;    // ends the active burst
    QTimer *m_sentryCooldownTimer = nullptr; // re-arms after the hold-off
    RepeaterProgram *m_repeaterProgram = nullptr;
    QWidget *m_repeaterScreen = nullptr; // back-bar wrapper around the Repeater
    bool m_repeaterActive = false;       // Repeater pipeline is running
    int m_repeaterRepeatCount = 0;
    IdentifierProgram *m_identifierProgram = nullptr;
    QWidget *m_identifierScreen = nullptr; // back-bar wrapper around Identifier
    bool m_identifierScanning = false;     // Identifier monitor is running
    bool m_identifierFound = false;        // a detection is latched on screen
    StreamParams m_identifierParams;       // band the identifier monitor scans
    // Burst capture: wait for a transmission, hold its peak envelope while it is
    // on air, then analyse once the channel has stayed quiet for the hang time.
    enum class IdentifyPhase { WaitSignal, Collecting };
    IdentifyPhase m_identifierPhase = IdentifyPhase::WaitSignal;
    float m_identifierThresholdDb = -20.0f;   // on/off level, latched at START
    QVector<float> m_identifierMaxHold;       // per-bin max over the burst
    std::chrono::steady_clock::time_point m_identifierLastSignal;
    std::chrono::steady_clock::time_point m_identifierBurstStart;
    // Quiet long enough to be sure the transmission (and its inter-packet gaps)
    // is over, and a ceiling so a continuous carrier still resolves.
    static constexpr auto kIdentifyQuiet = std::chrono::milliseconds(200);
    static constexpr auto kIdentifyMaxBurst = std::chrono::milliseconds(3000);
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
