#include "ui/MainWindow.h"

#include "dsp/SpectrumAccumulator.h"
#include "storage/IqFileReader.h"
#include "ui/CapturePanel.h"
#include "ui/DebugWorkspace.h"
#include "ui/DevicePanel.h"
#include "ui/PlaybackPanel.h"
#include "ui/SessionBrowser.h"
#include "ui/SpectrumWidget.h"
#include "ui/TransmitPanel.h"
#include "ui/WaterfallWidget.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDockWidget>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>

#include <algorithm>
#include <chrono>

namespace duality {

namespace {
constexpr int kCacheFftSize = 4096;
constexpr qint64 kCacheMaxSamples = 2 * 1024 * 1024;
}

MainWindow::MainWindow()
    : m_capture(&m_spectrumProcessor)
{
    setWindowTitle(QStringLiteral("DUALITY RF"));
    resize(1600, 920);

    // Central: live view (spectrum over waterfall) or debug workspace.
    m_spectrumView = new SpectrumWidget(this);
    m_waterfallView = new WaterfallWidget(this);
    auto *liveSplit = new QSplitter(Qt::Vertical, this);
    liveSplit->addWidget(m_spectrumView);
    liveSplit->addWidget(m_waterfallView);

    m_debugWorkspace = new DebugWorkspace(&m_store, this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(liveSplit);
    m_stack->addWidget(m_debugWorkspace);
    setCentralWidget(m_stack);

    createDocks();
    createToolbar();
    statusBar()->showMessage(tr("Ready"));

    // Live spectrum → views, plus peak detection while monitoring.
    connect(&m_spectrumProcessor, &SpectrumProcessor::spectrumReady, this,
            [this](const QVector<float> &row) {
                m_spectrumView->setTrace(row);
                m_waterfallView->addRow(row);
                if (!m_monitorLive)
                    return;
                QVector<DetectedPeak> appeared;
                const QVector<DetectedPeak> peaks =
                    m_peakDetector.update(row, &appeared);
                QVector<SpectrumMarker> markers;
                markers.reserve(peaks.size());
                for (const DetectedPeak &p : peaks)
                    markers.push_back({p.frequencyHz, p.powerDb});
                m_spectrumView->setMarkers(markers);
                for (const DetectedPeak &p : appeared) {
                    const QString entry =
                        tr("Peak detected: %1 MHz (%2 dB)")
                            .arg(p.frequencyHz / 1e6, 0, 'f', 4)
                            .arg(p.powerDb, 0, 'f', 1);
                    qInfo().noquote()
                        << QDateTime::currentDateTime().toString(Qt::ISODate)
                        << entry;
                    statusBar()->showMessage(entry, 3000);
                }
            });

    // Auto-capture keys off the instantaneous (un-averaged) frame so its
    // start/stop timing tracks the real envelope, not the smoothed trace.
    connect(&m_spectrumProcessor, &SpectrumProcessor::detectionRowReady, this,
            [this](const QVector<float> &row) {
                if (m_autoActive)
                    driveAutoCapture(row);
            });

    // Capture stage.
    connect(m_capturePanel, &CapturePanel::monitorRequested, this,
            [this] { startCapture(false); });
    connect(m_capturePanel, &CapturePanel::recordRequested, this,
            [this] { startCapture(true); });
    connect(m_capturePanel, &CapturePanel::stopRequested, this,
            &MainWindow::stopCapture);
    connect(m_capturePanel, &CapturePanel::paramsChanged, this,
            &MainWindow::updateCaptureRangeOverlay);
    connect(m_capturePanel, &CapturePanel::peakThresholdChanged, this,
            [this](double db) {
                m_peakDetector.setThresholdDb(static_cast<float>(db));
            });
    connect(&m_capture, &CapturePipeline::progress, this,
            [this](double sec, qint64 samples) {
                statusBar()->showMessage(
                    tr("Capturing: %1 s, %2 samples")
                        .arg(sec, 0, 'f', 1)
                        .arg(samples));
            });
    connect(&m_capture, &CapturePipeline::finished, this,
            &MainWindow::onCaptureFinished);
    connect(&m_capture, &CapturePipeline::segmentFinished, this,
            &MainWindow::onAutoSegmentSaved);
    connect(&m_capture, &CapturePipeline::segmentDiscarded, this,
            &MainWindow::rearmAuto);
    connect(&m_capture, &CapturePipeline::errorOccurred, this,
            [this](const QString &msg) {
                statusBar()->showMessage(msg);
                m_capturePanel->setRunning(false);
            });

    // Playback from the panel.
    connect(m_playbackPanel, &PlaybackPanel::playRequested, this, [this] {
        startPlayback(m_playbackPanel->recordingPath(),
                      m_playbackPanel->streamParams(),
                      m_playbackPanel->recordingMetadata().format,
                      m_playbackPanel->speed(), m_playbackPanel->repeat());
    });
    connect(m_playbackPanel, &PlaybackPanel::stopRequested, &m_playback,
            &PlaybackPipeline::stop);
    connect(&m_playback, &PlaybackPipeline::progress, m_playbackPanel,
            &PlaybackPanel::setProgress);
    connect(&m_playback, &PlaybackPipeline::finished, this, [this] {
        m_playbackPanel->setPlaying(false);
        statusBar()->showMessage(tr("Playback finished"));
    });
    connect(&m_playback, &PlaybackPipeline::errorOccurred, this,
            [this](const QString &msg) {
                m_playbackPanel->setPlaying(false);
                statusBar()->showMessage(msg);
            });

    // Session browser → playback panel; debug replay → playback pipeline.
    connect(m_sessionBrowser, &SessionBrowser::recordingSelected, this,
            [this](const QString &sessionDir, const RecordingMetadata &meta) {
                m_playbackPanel->setRecording(meta,
                                              sessionDir + '/' + meta.file);
            });
    connect(m_sessionBrowser, &SessionBrowser::newSessionRequested, this,
            [this] {
                m_session = m_store.createSession();
                m_sessionBrowser->refresh();
                m_debugWorkspace->refreshSessions();
                statusBar()->showMessage(
                    tr("Created %1").arg(m_session->name()));
            });
    connect(m_debugWorkspace, &DebugWorkspace::replayRequested, this,
            [this](const QString &absPath, const RecordingMetadata &meta) {
                StreamParams p;
                p.frequencyHz = meta.frequencyHz;
                p.sampleRateHz = meta.sampleRateHz;
                p.gainDb = 20.0;
                startPlayback(absPath, p, meta.format, 1.0, false);
            });

    connect(&m_transmit, &TransmitPipeline::errorOccurred, this,
            [this](const QString &msg) { statusBar()->showMessage(msg); });
    connect(m_transmitPanel, &TransmitPanel::enabledChanged, this,
            &MainWindow::onTransmitToggled);

    m_deviceManager.rescan();
    updateCaptureRangeOverlay();
}

MainWindow::~MainWindow()
{
    m_capture.stop();
    m_transmit.stop();
    m_playback.stop();
}

void MainWindow::createDocks()
{
    m_devicePanel = new DevicePanel(&m_deviceManager, this);
    m_capturePanel = new CapturePanel(this);
    m_transmitPanel = new TransmitPanel(this);
    m_playbackPanel = new PlaybackPanel(this);
    m_sessionBrowser = new SessionBrowser(&m_store, this);

    const auto addDock = [this](const QString &title, QWidget *widget,
                                Qt::DockWidgetArea area, bool scroll,
                                int minWidth) {
        auto *dock = new QDockWidget(title, this);
        dock->setObjectName(title);
        // The window style paints the default title text and ignores the
        // stylesheet color, so use a plain label as the title bar to guarantee
        // black-on-white headers.
        auto *titleBar = new QLabel(title, dock);
        titleBar->setStyleSheet(QStringLiteral(
            "background:#ffffff; color:#000000; padding:6px; "
            "border:1px solid #ffffff; font-weight:bold;"));
        dock->setTitleBarWidget(titleBar);
        if (scroll) {
            // Wrap tall settings panels so the left column can scroll instead
            // of squeezing the controls when several docks stack up.
            auto *scrollArea = new QScrollArea(dock);
            scrollArea->setWidget(widget);
            scrollArea->setWidgetResizable(true);
            scrollArea->setFrameShape(QFrame::NoFrame);
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            dock->setWidget(scrollArea);
        } else {
            dock->setWidget(widget);
        }
        // A minimum on the dock is honored by the layout (a minimum on a panel
        // inside a scroll area is not), so this reliably fixes the column
        // widths.
        dock->setMinimumWidth(minWidth);
        addDockWidget(area, dock);
        return dock;
    };
    QDockWidget *devicesDock =
        addDock(tr("DEVICES"), m_devicePanel, Qt::LeftDockWidgetArea, true, 520);
    addDock(tr("CAPTURE"), m_capturePanel, Qt::LeftDockWidgetArea, true, 520);
    addDock(tr("CONCURRENT TX"), m_transmitPanel, Qt::LeftDockWidgetArea, true,
            520);
    addDock(tr("SESSIONS"), m_sessionBrowser, Qt::RightDockWidgetArea, false,
            380);
    addDock(tr("PLAYBACK"), m_playbackPanel, Qt::RightDockWidgetArea, false,
            380);

    QSettings settings;
    restoreGeometry(
        settings.value(QStringLiteral("geometry/v5")).toByteArray());
    const QByteArray state =
        settings.value(QStringLiteral("windowState/v5")).toByteArray();
    if (state.isEmpty() || !restoreState(state))
        resizeDocks({devicesDock}, {640}, Qt::Horizontal);
}

void MainWindow::createToolbar()
{
    auto *bar = addToolBar(tr("Views"));
    bar->setObjectName(QStringLiteral("views"));
    bar->setMovable(false);

    auto *title = new QLabel(QStringLiteral("  DUALITY RF  "), this);
    bar->addWidget(title);

    QAction *liveAction = bar->addAction(tr("CAPTURE"));
    QAction *debugAction = bar->addAction(tr("DEBUG"));
    liveAction->setCheckable(true);
    debugAction->setCheckable(true);
    liveAction->setChecked(true);
    connect(liveAction, &QAction::triggered, this, [=, this] {
        m_stack->setCurrentIndex(0);
        liveAction->setChecked(true);
        debugAction->setChecked(false);
    });
    connect(debugAction, &QAction::triggered, this, [=, this] {
        m_stack->setCurrentIndex(1);
        m_debugWorkspace->refreshSessions();
        liveAction->setChecked(false);
        debugAction->setChecked(true);
    });
}

Session *MainWindow::ensureSession()
{
    if (!m_session) {
        m_session = m_store.createSession();
        m_sessionBrowser->refresh();
    }
    return m_session.get();
}

void MainWindow::startCapture(bool record)
{
    const auto rxInfo = m_devicePanel->selectedRx();
    if (!rxInfo) {
        statusBar()->showMessage(tr("Select a receiver first"));
        return;
    }
    auto rxDevice = m_deviceManager.open(*rxInfo);
    if (!rxDevice) {
        statusBar()->showMessage(tr("Cannot open %1").arg(rxInfo->displayName()));
        return;
    }

    // Auto trigger + RECORD arms continuous per-transmission capture: the
    // pipeline runs as a monitor and segments are carved out on detection.
    const bool autoMode =
        record && m_capturePanel->trigger() == QLatin1String("auto");

    CapturePipeline::Plan plan;
    plan.params = m_capturePanel->streamParams();
    plan.trigger = m_capturePanel->trigger();
    plan.captureRangeHz = m_capturePanel->captureRangeHz();
    if (autoMode) {
        const double rate = plan.params.sampleRateHz;
        const auto msToSamples = [rate](std::chrono::milliseconds ms) {
            return static_cast<qint64>(rate * ms.count() / 1000.0);
        };
        plan.preRollSamples = msToSamples(kAutoPreRoll);
        plan.minSegmentSamples = msToSamples(kAutoMinSegment);
    }
    if (record && !autoMode) {
        plan.durationSec = m_capturePanel->durationSec();
        plan.outputPath = ensureSession()->nextRecordingPath();
    }

    // Optional concurrent waveform transmission on the same channel; runs for
    // both monitor and record stages and can be toggled live (see
    // onTransmitToggled).
    if (m_transmitPanel->transmitEnabled()) {
        if (!startConcurrentTx(plan.params))
            return;
        const WaveformConfig waveform = m_transmitPanel->waveformConfig();
        plan.concurrentTx = waveform.describe();
        plan.txGainDb = m_transmitPanel->txGainDb();
    }

    if (!m_capture.start(std::move(rxDevice), plan)) {
        m_transmit.stop();
        return;
    }
    m_activeCaptureParams = plan.params;
    m_spectrumView->setAxis(plan.params.frequencyHz, plan.params.sampleRateHz);
    m_spectrumView->setMarkers({});
    m_waterfallView->clear();
    updateCaptureRangeOverlay();
    m_peakDetector.setAxis(plan.params.frequencyHz, plan.params.sampleRateHz);
    m_peakDetector.setThresholdDb(
        static_cast<float>(m_capturePanel->peakThresholdDb()));
    m_peakDetector.reset();
    // Peak markers run for monitor and auto (both keep the live spectrum).
    m_monitorLive = !record || autoMode;
    if (autoMode)
        startAutoCapture();
    // Replay mode (auto record only): count segments so the second one triggers
    // the switch to monitor-and-replay.
    m_replayActive = record && autoMode && m_capturePanel->replayMode();
    m_replaySegmentCount = 0;
    m_replayFirstPath.clear();
    m_capturePanel->setRunning(true);
    statusBar()->showMessage(autoMode ? tr("Auto capture armed…")
                                      : record ? tr("Recording…")
                                               : tr("Monitoring…"));
}

void MainWindow::startAutoCapture()
{
    m_autoActive = true;
    m_autoPhase = AutoPhase::WaitSignal;
    const StreamParams p = m_capturePanel->streamParams();
    m_autoBandCenterHz = p.frequencyHz;
    m_autoBandHalfHz = m_capturePanel->captureRangeHz();
    m_autoSampleRateHz = p.sampleRateHz;
    m_autoOnThresholdDb = static_cast<float>(m_capturePanel->peakThresholdDb());
    m_autoOffThresholdDb = m_autoOnThresholdDb - kAutoHysteresisDb;
    m_autoHangTime = std::chrono::milliseconds(
        static_cast<qint64>(m_capturePanel->hangTimeMs()));
    ensureSession();
}

void MainWindow::driveAutoCapture(const QVector<float> &row)
{
    const auto now = std::chrono::steady_clock::now();
    switch (m_autoPhase) {
    case AutoPhase::WaitSignal:
        // Open on the higher on-threshold so noise near the level does not arm.
        if (m_peakDetector.signalPresent(row, m_autoBandCenterHz,
                                         m_autoBandHalfHz,
                                         m_autoOnThresholdDb)) {
            m_capture.beginSegment(ensureSession()->nextRecordingPath());
            m_autoPhase = AutoPhase::Recording;
            m_autoLastSignal = now;
            statusBar()->showMessage(tr("Capturing transmission…"));
        }
        break;
    case AutoPhase::Recording:
        // Hold open on the lower off-threshold (hysteresis) and only close
        // after the signal has stayed gone for the full hang time.
        if (m_peakDetector.signalPresent(row, m_autoBandCenterHz,
                                         m_autoBandHalfHz,
                                         m_autoOffThresholdDb)) {
            m_autoLastSignal = now;
        } else if (now - m_autoLastSignal >= m_autoHangTime) {
            // Drop the silence held open past the signal (the hang window minus
            // a small post-roll) so the stored file ends near the transmission.
            const auto silence =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - m_autoLastSignal);
            const auto drop = silence - std::min(silence, kAutoPostPad);
            const qint64 dropSamples = static_cast<qint64>(
                m_autoSampleRateHz * drop.count() / 1000.0);
            m_capture.endSegment(dropSamples);
            m_autoPhase = AutoPhase::Finalizing;
        }
        break;
    case AutoPhase::Finalizing:
        break; // waiting for the worker to trim + write the segment
    }
}

void MainWindow::onAutoSegmentSaved(const RecordingMetadata &meta)
{
    Session *session = ensureSession();
    if (session) {
        session->addRecording(meta);
        cacheRecordingSpectrum(meta);
        m_sessionBrowser->refresh();
    }
    const QString when =
        meta.startedUtc.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    qInfo().noquote() << QDateTime::currentDateTime().toString(Qt::ISODate)
                      << "Auto capture saved" << meta.file << "at" << when;
    statusBar()->showMessage(tr("Saved %1 at %2 (%3 s, %4 kHz band)")
                                 .arg(meta.file)
                                 .arg(when)
                                 .arg(meta.durationSec, 0, 'f', 2)
                                 .arg(meta.bandwidthHz / 1e3, 0, 'f', 0));

    if (m_replayActive) {
        ++m_replaySegmentCount;
        if (m_replaySegmentCount == 1) {
            // Remember the first signal to replay once the second arrives.
            m_replayFirstMeta = meta;
            m_replayFirstPath = session ? session->recordingPath(meta) : QString();
        } else if (m_replaySegmentCount >= 2) {
            enterReplayMonitor(); // leaves auto capture; do not re-arm
            return;
        }
    }
    rearmAuto();
}

void MainWindow::enterReplayMonitor()
{
    m_replayActive = false;

    // Drop the concurrent TX noise (and reflect it in the panel). Unticking
    // while the capture is live stops the stream via onTransmitToggled; the
    // extra stop() covers the case where it was never enabled.
    if (m_transmitPanel->transmitEnabled())
        m_transmitPanel->setTransmitEnabled(false);
    m_transmit.stop();

    // Leave segment capture: the pipeline keeps receiving, now a plain monitor.
    m_autoActive = false;
    m_autoPhase = AutoPhase::WaitSignal;

    // Replay the first captured signal on the transmitter.
    if (m_replayFirstPath.isEmpty()) {
        statusBar()->showMessage(tr("Replay mode: no signal to replay"));
        return;
    }
    m_playbackPanel->setRecording(m_replayFirstMeta, m_replayFirstPath);
    startPlayback(m_replayFirstPath, m_playbackPanel->streamParams(),
                  m_replayFirstMeta.format, m_playbackPanel->speed(),
                  m_playbackPanel->repeat());
}

void MainWindow::rearmAuto()
{
    // Re-arm for the next transmission; a segment finalized during stop
    // (m_autoActive already false) does not re-arm.
    if (m_autoActive)
        m_autoPhase = AutoPhase::WaitSignal;
}

bool MainWindow::startConcurrentTx(const StreamParams &captureParams)
{
    const auto txInfo = m_devicePanel->selectedTx();
    if (!txInfo) {
        statusBar()->showMessage(
            tr("Concurrent TX enabled but no transmitter selected"));
        return false;
    }
    auto txDevice = m_deviceManager.open(*txInfo);
    if (!txDevice) {
        statusBar()->showMessage(
            tr("Cannot open %1").arg(txInfo->displayName()));
        return false;
    }
    StreamParams txParams = captureParams;
    txParams.gainDb = m_transmitPanel->txGainDb();
    const WaveformConfig waveform = m_transmitPanel->waveformConfig();
    return m_transmit.start(std::move(txDevice), txParams, waveform);
}

void MainWindow::onTransmitToggled(bool enabled)
{
    // Only act while a capture is live; otherwise the toggle just configures
    // what the next capture will start with.
    if (!m_capture.running())
        return;
    if (enabled) {
        if (startConcurrentTx(m_activeCaptureParams))
            statusBar()->showMessage(tr("Concurrent TX started"));
    } else {
        m_transmit.stop();
        statusBar()->showMessage(tr("Concurrent TX stopped"));
    }
}

void MainWindow::stopCapture()
{
    // Clear the auto flag first so a segment finalized during stop is kept
    // but does not re-arm the state machine.
    m_autoActive = false;
    m_autoPhase = AutoPhase::WaitSignal;
    m_replayActive = false;
    m_capture.stop();
    m_transmit.stop();
    m_monitorLive = false;
    // Peak markers live for the duration of one monitor stage.
    m_peakDetector.reset();
    m_spectrumView->setMarkers({});
    m_capturePanel->setRunning(false);
    updateCaptureRangeOverlay();
    statusBar()->showMessage(tr("Stopped"));
}

void MainWindow::updateCaptureRangeOverlay()
{
    const StreamParams p = m_capturePanel->streamParams();
    // While idle the spectrum has no axis yet; drive it from the panel so the
    // capture-range markers render before any capture starts.
    if (!m_capture.running())
        m_spectrumView->setAxis(p.frequencyHz, p.sampleRateHz);
    // Two markers at frequency ± capture range delimit what a capture keeps.
    m_spectrumView->setCaptureRange(p.frequencyHz,
                                    2.0 * m_capturePanel->captureRangeHz());
}

void MainWindow::onCaptureFinished(const RecordingMetadata &meta,
                                   bool wasRecording)
{
    m_transmit.stop();
    m_monitorLive = false;
    m_capturePanel->setRunning(false);
    if (!wasRecording) {
        statusBar()->showMessage(tr("Monitor stopped"));
        return;
    }
    if (Session *session = ensureSession()) {
        session->addRecording(meta);
        cacheRecordingSpectrum(meta);
        m_sessionBrowser->refresh();
        statusBar()->showMessage(tr("Saved %1 (%2 s)")
                                     .arg(meta.file)
                                     .arg(meta.durationSec, 0, 'f', 1));
    }
}

void MainWindow::cacheRecordingSpectrum(const RecordingMetadata &meta)
{
    IqFileReader reader;
    if (!reader.open(m_session->recordingPath(meta), meta.format))
        return;
    SpectrumAccumulator acc(kCacheFftSize);
    std::vector<Complex> chunk(65536);
    qint64 remaining = std::min(reader.totalSamples(), kCacheMaxSamples);
    while (remaining > 0) {
        const std::size_t got = reader.read(std::span(
            chunk.data(),
            std::min<qint64>(remaining, static_cast<qint64>(chunk.size()))));
        if (got == 0)
            break;
        acc.process(std::span(chunk.data(), got));
        remaining -= static_cast<qint64>(got);
    }
    if (acc.blocks() > 0)
        m_session->fftCache().append(meta.file, acc.averageDb());
}

void MainWindow::startPlayback(const QString &absPath,
                               const StreamParams &params, SampleFormat format,
                               double speed, bool repeat)
{
    const auto txInfo = m_devicePanel->selectedTx();
    if (!txInfo) {
        statusBar()->showMessage(tr("Select a transmitter first"));
        return;
    }
    auto txDevice = m_deviceManager.open(*txInfo);
    if (!txDevice) {
        statusBar()->showMessage(tr("Cannot open %1").arg(txInfo->displayName()));
        return;
    }

    PlaybackPipeline::Params p;
    p.stream = params;
    p.format = format;
    p.speed = speed;
    p.repeat = repeat;
    if (m_playback.start(std::move(txDevice), absPath, p)) {
        m_playbackPanel->setPlaying(true);
        statusBar()->showMessage(tr("Transmitting %1").arg(absPath));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(QStringLiteral("geometry/v5"), saveGeometry());
    settings.setValue(QStringLiteral("windowState/v5"), saveState());
    m_capture.stop();
    m_transmit.stop();
    m_playback.stop();
    QMainWindow::closeEvent(event);
}

} // namespace duality
