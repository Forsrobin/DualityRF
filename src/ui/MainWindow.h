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
class DebugWorkspace;
class DevicePanel;
class PlaybackPanel;
class SessionBrowser;
class SpectrumWidget;
class TransmitPanel;
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
    void createDocks();
    void createToolbar();
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
    std::chrono::steady_clock::time_point m_autoLastSignal;
    static constexpr auto kAutoDebounce = std::chrono::milliseconds(300);
    CapturePipeline m_capture;
    PlaybackPipeline m_playback;
    TransmitPipeline m_transmit;

    // Views
    QStackedWidget *m_stack;
    SpectrumWidget *m_spectrumView;
    WaterfallWidget *m_waterfallView;
    DebugWorkspace *m_debugWorkspace;

    // Panels
    DevicePanel *m_devicePanel;
    CapturePanel *m_capturePanel;
    TransmitPanel *m_transmitPanel;
    PlaybackPanel *m_playbackPanel;
    SessionBrowser *m_sessionBrowser;
};

} // namespace duality
