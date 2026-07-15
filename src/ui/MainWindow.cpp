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
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>

namespace duality {

namespace {
constexpr int kCacheFftSize = 4096;
constexpr qint64 kCacheMaxSamples = 2 * 1024 * 1024;
}

MainWindow::MainWindow()
    : m_capture(&m_spectrumProcessor)
{
    setWindowTitle(QStringLiteral("DUALITY RF"));
    resize(1280, 800);

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

    m_deviceManager.rescan();
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
                                Qt::DockWidgetArea area) {
        auto *dock = new QDockWidget(title, this);
        dock->setObjectName(title);
        dock->setWidget(widget);
        addDockWidget(area, dock);
        return dock;
    };
    QDockWidget *devicesDock =
        addDock(tr("DEVICES"), m_devicePanel, Qt::LeftDockWidgetArea);
    addDock(tr("CAPTURE"), m_capturePanel, Qt::LeftDockWidgetArea);
    addDock(tr("CONCURRENT TX"), m_transmitPanel, Qt::LeftDockWidgetArea);
    addDock(tr("SESSIONS"), m_sessionBrowser, Qt::RightDockWidgetArea);
    addDock(tr("PLAYBACK"), m_playbackPanel, Qt::RightDockWidgetArea);

    // Keep the settings column comfortable regardless of layout state.
    m_devicePanel->setMinimumWidth(280);
    m_capturePanel->setMinimumWidth(280);
    m_transmitPanel->setMinimumWidth(280);

    QSettings settings;
    restoreGeometry(
        settings.value(QStringLiteral("geometry/v2")).toByteArray());
    const QByteArray state =
        settings.value(QStringLiteral("windowState/v2")).toByteArray();
    if (state.isEmpty() || !restoreState(state))
        resizeDocks({devicesDock}, {380}, Qt::Horizontal);
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

    CapturePipeline::Plan plan;
    plan.params = m_capturePanel->streamParams();
    plan.durationSec = record ? m_capturePanel->durationSec() : 0.0;
    plan.trigger = m_capturePanel->trigger();
    if (record)
        plan.outputPath = ensureSession()->nextRecordingPath();

    // Optional concurrent waveform transmission on the same channel; runs
    // for both monitor and record stages.
    if (m_transmitPanel->transmitEnabled()) {
        const auto txInfo = m_devicePanel->selectedTx();
        if (!txInfo) {
            statusBar()->showMessage(
                tr("Concurrent TX enabled but no transmitter selected"));
            return;
        }
        auto txDevice = m_deviceManager.open(*txInfo);
        if (!txDevice) {
            statusBar()->showMessage(
                tr("Cannot open %1").arg(txInfo->displayName()));
            return;
        }
        StreamParams txParams = plan.params;
        txParams.gainDb = m_transmitPanel->txGainDb();
        const WaveformConfig waveform = m_transmitPanel->waveformConfig();
        if (!m_transmit.start(std::move(txDevice), txParams, waveform))
            return;
        plan.concurrentTx = waveform.describe();
        plan.txGainDb = txParams.gainDb;
    }

    if (!m_capture.start(std::move(rxDevice), plan)) {
        m_transmit.stop();
        return;
    }
    m_spectrumView->setAxis(plan.params.frequencyHz, plan.params.sampleRateHz);
    m_spectrumView->setMarkers({});
    m_waterfallView->clear();
    updateCaptureRangeOverlay();
    m_peakDetector.setAxis(plan.params.frequencyHz, plan.params.sampleRateHz);
    m_peakDetector.setThresholdDb(
        static_cast<float>(m_capturePanel->peakThresholdDb()));
    m_peakDetector.reset();
    m_monitorLive = !record;
    m_capturePanel->setRunning(true);
    statusBar()->showMessage(record ? tr("Recording…") : tr("Monitoring…"));
}

void MainWindow::stopCapture()
{
    m_capture.stop();
    m_transmit.stop();
    m_monitorLive = false;
    // Peak markers live for the duration of one monitor stage.
    m_peakDetector.reset();
    m_spectrumView->setMarkers({});
    m_capturePanel->setRunning(false);
    statusBar()->showMessage(tr("Stopped"));
}

void MainWindow::updateCaptureRangeOverlay()
{
    // Bandwidth "auto" (0) records the full sample-rate span.
    const StreamParams p = m_capturePanel->streamParams();
    const double widthHz =
        p.bandwidthHz > 0.0 ? p.bandwidthHz : p.sampleRateHz;
    m_spectrumView->setCaptureRange(p.frequencyHz, widthHz);
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
    settings.setValue(QStringLiteral("geometry/v2"), saveGeometry());
    settings.setValue(QStringLiteral("windowState/v2"), saveState());
    m_capture.stop();
    m_transmit.stop();
    m_playback.stop();
    QMainWindow::closeEvent(event);
}

} // namespace duality
