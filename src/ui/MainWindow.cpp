#include "ui/MainWindow.h"

#include "dsp/SpectrumAccumulator.h"
#include "storage/IqFileReader.h"
#include "ui/panels/CaptureControlPanel.h"
#include "ui/panels/CapturePanel.h"
#include "ui/panels/LookingPanel.h"
#include "ui/HomePage.h"
#include "ui/panels/PlaybackPanel.h"
#include "ui/ProgramScreen.h"
#include "ui/panels/SessionList.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/SplashPage.h"
#include "ui/Theme.h"
#include "ui/components/ToastManager.h"
#include "ui/panels/TransmitPanel.h"
#include "ui/widgets/WaterfallWidget.h"
#include "ui/programs/CaptureProgram.h"
#include "ui/programs/DebugProgram.h"
#include "ui/programs/DevicesProgram.h"
#include "ui/programs/FobProgram.h"
#include "ui/programs/InfoProgram.h"
#include "ui/programs/JamProgram.h"
#include "ui/programs/LookingProgram.h"
#include "ui/programs/SessionsProgram.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <chrono>

namespace duality {

namespace {
constexpr int kCacheFftSize = 4096;
constexpr qint64 kCacheMaxSamples = 2 * 1024 * 1024;
// Pause after stopping concurrent TX before the replay grabs the transmitter,
// so the TX stream fully stops and releases the device first.
constexpr int kReplayTxSettleMs = 750;
// How long the in-app boot splash stays up before revealing the home grid.
constexpr int kSplashMinimumMs = 1500;
} // namespace

MainWindow::MainWindow()
    : m_capture(&m_spectrumProcessor), m_jamMonitor(&m_jamProcessor) {
  setWindowTitle(QStringLiteral("DUALITY RF"));

  // Fixed 320x480 portrait popout: not resizable. Equal min/max size is also
  // what makes Wayland tiling compositors (e.g. Hyprland) float it as a popup
  // instead of tiling it to fill the output.
  setFixedSize(kWindowWidth, kWindowHeight);

  // Scaled-down stylesheet sized for the 320x480 popout.
  Theme::apply(*qApp, true);

  // Central stack: the home grid (index 0) launches each program screen.
  m_debugProgram = new DebugProgram(&m_store, this);
  m_infoProgram = new InfoProgram(this);

  m_stack = new QStackedWidget(this);
  setCentralWidget(m_stack);

  m_home = new HomePage(this);
  m_stack->addWidget(m_home);
  connect(m_home, &HomePage::exitRequested, qApp, &QApplication::quit);
  // The home top bar mirrors the program screens' device badges (no back arrow).
  connect(m_home, &HomePage::txClicked, this,
          [this] { openDevices(DeviceFocus::Tx); });
  connect(m_home, &HomePage::rxClicked, this,
          [this] { openDevices(DeviceFocus::Rx); });
  connect(m_home, &HomePage::programActivated, this, [this](int index) {
    const int target = index + 1; // stack index (home is 0)
    // The device selector and session manager use history-aware navigation;
    // everything else just switches straight to its screen.
    if (target == m_devicesIndex)
      openDevices(DeviceFocus::None);
    else if (target == m_sessionsIndex)
      openSessions(QString());
    else
      m_stack->setCurrentIndex(target);
  });

  // App-wide device selector; the pipelines read its selection and every
  // program's top bar mirrors it.
  m_devicesProgram = new DevicesProgram(&m_deviceManager, this);

  // Register programs — add a line here to add a new screen. Devicees comes
  // first so device selection is the front tile on the home grid.
  ProgramScreen *devicesScreen = addProgram(
      tr("DEVICEES"), QStringLiteral(":/assets/devicees.png"), m_devicesProgram);
  m_devicesIndex = m_stack->indexOf(devicesScreen);
  // Override the default back (home): return to whichever screen opened it.
  disconnect(devicesScreen, &ProgramScreen::backRequested, nullptr, nullptr);
  connect(devicesScreen, &ProgramScreen::backRequested, this,
          [this] { m_stack->setCurrentIndex(m_devicesReturnIndex); });

  // The FOB program must be built before the panel signals are wired below
  // (it owns them).
  m_fobProgram = new FobProgram(&m_store, this);
  // Keep alias pointers so the rest of the wiring reads the panels directly.
  m_capturePanel = m_fobProgram->capturePanel();
  m_activeCapture = m_capturePanel; // default source until a program arms it
  m_transmitPanel = m_fobProgram->transmitPanel();
  m_fobSessionList = m_fobProgram->sessionList();
  m_spectrumViews.push_back(m_fobProgram->spectrumView());
  m_waterfallViews.push_back(m_fobProgram->waterfallView());
  addProgram(tr("FOB"), QStringLiteral(":/assets/fob.png"), m_fobProgram);

  // Standalone CAPTURE program: a stripped-down FOB (capture + sessions) over
  // the same waterfall/FFT view, driving the shared capture pipeline. Its views
  // join the broadcast list so the live capture renders here too.
  m_captureProgram = new CaptureProgram(&m_store, this);
  m_captureSessionList = m_captureProgram->sessionList();
  m_spectrumViews.push_back(m_captureProgram->spectrumView());
  m_waterfallViews.push_back(m_captureProgram->waterfallView());
  addProgram(tr("CAPTURE"), QStringLiteral(":/assets/capture.png"),
             m_captureProgram);

  // LOOKING glass: monitor-only analysis tool over the shared pipeline and the
  // same broadcast views.
  m_lookingProgram = new LookingProgram(this);
  m_lookingPanel = m_lookingProgram->panel();
  m_spectrumViews.push_back(m_lookingProgram->spectrumView());
  m_waterfallViews.push_back(m_lookingProgram->waterfallView());
  addProgram(tr("LOOKING"), QStringLiteral(":/assets/looking_glass.png"),
             m_lookingProgram);

  // Full session manager. Like the device selector it uses return-to-caller
  // navigation so a session tile on FOB/CAPTURE comes back to that program.
  m_sessionsProgram = new SessionsProgram(&m_store, this);
  // The only playback panel now lives in the Sessions manager; the pipeline
  // wiring below drives it.
  m_playbackPanel = m_sessionsProgram->playbackPanel();
  m_activePlayback = m_playbackPanel;
  ProgramScreen *sessionsScreen = addProgram(
      tr("SESSIONS"), QStringLiteral(":/assets/sessions.png"), m_sessionsProgram);
  m_sessionsIndex = m_stack->indexOf(sessionsScreen);
  disconnect(sessionsScreen, &ProgramScreen::backRequested, nullptr, nullptr);
  connect(sessionsScreen, &ProgramScreen::backRequested, this,
          [this] { m_stack->setCurrentIndex(m_sessionsReturnIndex); });

  m_jamProgram = new JamProgram(this);
  m_jamScreen = addProgram(tr("JAM"), QStringLiteral(":/assets/jam.png"),
                           m_jamProgram);
  m_debugScreen = addProgram(tr("DEBUG"), QStringLiteral(":/assets/bug.png"),
                             m_debugProgram);
  addProgram(tr("ABOUT"), QStringLiteral(":/assets/info.png"), m_infoProgram);
  m_home->addExitTile();

  // Keep the top-bar device badges in sync with the selector.
  connect(m_devicesProgram, &DevicesProgram::selectionChanged, this,
          &MainWindow::refreshDeviceBadges);
  refreshDeviceBadges();

  // Only program screens carry a status bar; the home grid has its own footer
  // and the splash is chrome-free. Refresh debug sessions when it opens.
  connect(m_stack, &QStackedWidget::currentChanged, this, [this] {
    statusBar()->setVisible(
        qobject_cast<ProgramScreen *>(m_stack->currentWidget()) != nullptr);
    if (m_stack->currentWidget() == m_debugScreen)
      m_debugProgram->refreshSessions();
    // Leaving the Jam screen stops its transmission so it can't run unseen.
    if (m_jamActive && m_stack->currentWidget() != m_jamScreen)
      stopJam();
  });

  // Boot splash lives in the stack too; show it first, then reveal the home
  // grid after a short minimum so it doesn't just flash.
  m_splash = new SplashPage(this);
  m_stack->addWidget(m_splash);
  m_stack->setCurrentWidget(m_splash);
  QTimer::singleShot(kSplashMinimumMs, this,
                     [this] { m_stack->setCurrentWidget(m_home); });

  m_toasts = new ToastManager(this, this);
  statusBar()->showMessage(tr("Ready"));
  statusBar()->hide(); // splash/home carry no status bar

  // Live spectrum → views, plus peak detection while monitoring.
  connect(&m_spectrumProcessor, &SpectrumProcessor::spectrumReady, this,
          [this](const QVector<float> &row) {
            for (SpectrumWidget *view : m_spectrumViews)
              view->setTrace(row);
            for (WaterfallWidget *view : m_waterfallViews)
              view->addRow(row);
            if (!m_monitorLive || !m_peakMarkersEnabled)
              return;
            QVector<DetectedPeak> appeared;
            const QVector<DetectedPeak> peaks =
                m_peakDetector.update(row, &appeared);
            QVector<SpectrumMarker> markers;
            markers.reserve(peaks.size());
            for (const DetectedPeak &p : peaks)
              markers.push_back({p.frequencyHz, p.powerDb});
            for (SpectrumWidget *view : m_spectrumViews)
              view->setMarkers(markers);
            for (const DetectedPeak &p : appeared) {
              const QString entry = tr("Peak detected: %1 MHz (%2 dB)")
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

  // Capture stage. Each arming button first claims the pipeline for its own
  // program's controls; the guard blocks starting a second capture while one
  // from either program is already live.
  connect(m_capturePanel, &CapturePanel::monitorRequested, this, [this] {
    if (m_capture.running())
      return;
    setActiveCapture(m_capturePanel, true);
    startCapture(false);
  });
  connect(m_capturePanel, &CapturePanel::recordRequested, this, [this] {
    if (m_capture.running())
      return;
    setActiveCapture(m_capturePanel, true);
    startCapture(true);
  });
  connect(m_capturePanel, &CapturePanel::stopRequested, this,
          &MainWindow::stopCapture);

  // CAPTURE program: same pipeline, simplified controls (no monitor, no
  // concurrent TX, no replay).
  CaptureControlPanel *cc = m_captureProgram->capturePanel();
  connect(cc, &CaptureControlPanel::recordRequested, this, [this, cc] {
    if (m_capture.running())
      return;
    setActiveCapture(cc, false);
    startCapture(true);
  });
  connect(cc, &CaptureControlPanel::stopRequested, this,
          &MainWindow::stopCapture);
  connect(cc, &CaptureControlPanel::paramsChanged, this,
          &MainWindow::updateCaptureRangeOverlay);
  connect(cc, &CaptureControlPanel::peakThresholdChanged, this,
          [this](double db) {
            m_peakDetector.setThresholdDb(static_cast<float>(db));
          });
  connect(m_capturePanel, &CapturePanel::paramsChanged, this,
          &MainWindow::updateCaptureRangeOverlay);
  connect(m_capturePanel, &CapturePanel::peakThresholdChanged, this,
          [this](double db) {
            m_peakDetector.setThresholdDb(static_cast<float>(db));
          });

  // LOOKING glass: monitor only, one START/STOP button, peak detection toggle.
  connect(m_lookingPanel, &LookingPanel::startRequested, this, [this] {
    if (m_capture.running())
      return;
    setActiveCapture(m_lookingPanel, false);
    startCapture(false);
  });
  connect(m_lookingPanel, &LookingPanel::stopRequested, this,
          &MainWindow::stopCapture);
  connect(m_lookingPanel, &LookingPanel::paramsChanged, this,
          &MainWindow::updateCaptureRangeOverlay);
  connect(m_lookingPanel, &LookingPanel::peakThresholdChanged, this,
          [this](double db) {
            m_peakDetector.setThresholdDb(static_cast<float>(db));
          });
  // Toggle peak markers live while monitoring; clear them when turned off.
  connect(m_lookingPanel, &LookingPanel::peakDetectionToggled, this,
          [this](bool on) {
            m_peakMarkersEnabled = on;
            if (!on) {
              m_peakDetector.reset();
              for (SpectrumWidget *view : m_spectrumViews)
                view->setMarkers({});
            }
          });
  connect(
      &m_capture, &CapturePipeline::progress, this,
      [this](double sec, qint64 samples) {
        statusBar()->showMessage(
            tr("Capturing: %1 s, %2 samples").arg(sec, 0, 'f', 1).arg(samples));
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
            if (m_activeCapture)
              m_activeCapture->setRunning(false);
          });

  // Playback: both the FOB playback tab and the Sessions manager's player feed
  // the one shared pipeline. Whichever asks to play becomes the active panel so
  // progress/finish routes back to it.
  const auto wirePlayback = [this](PlaybackPanel *panel) {
    connect(panel, &PlaybackPanel::playRequested, this, [this, panel] {
      m_activePlayback = panel;
      startPlayback(panel->recordingPath(), panel->streamParams(),
                    panel->recordingMetadata().format, panel->speed(),
                    panel->repeat());
    });
    connect(panel, &PlaybackPanel::stopRequested, &m_playback,
            &PlaybackPipeline::stop);
  };
  wirePlayback(m_playbackPanel);
  connect(&m_playback, &PlaybackPipeline::progress, this,
          [this](double sec, double total) {
            if (m_activePlayback)
              m_activePlayback->setProgress(sec, total);
          });
  connect(&m_playback, &PlaybackPipeline::finished, this, [this] {
    if (m_activePlayback)
      m_activePlayback->setPlaying(false);
    statusBar()->showMessage(tr("Playback finished"));
  });
  connect(&m_playback, &PlaybackPipeline::errorOccurred, this,
          [this](const QString &msg) {
            if (m_activePlayback)
              m_activePlayback->setPlaying(false);
            statusBar()->showMessage(msg);
          });

  // Tapping a session tile on FOB/CAPTURE opens it in the Sessions manager.
  connect(m_fobSessionList, &SessionList::sessionActivated, this,
          &MainWindow::openSessions);
  connect(m_captureSessionList, &SessionList::sessionActivated, this,
          &MainWindow::openSessions);

  // Session create/delete from the manager → refresh every view.
  const auto onNewSession = [this] {
    m_session = m_store.createSession();
    refreshBrowsers();
    m_debugProgram->refreshSessions();
    m_playbackPanel->refresh();
    statusBar()->showMessage(tr("Created %1").arg(m_session->name()));
  };
  connect(m_sessionsProgram, &SessionsProgram::newSessionRequested, this,
          onNewSession);
  connect(m_sessionsProgram, &SessionsProgram::sessionsChanged, this, [this] {
    refreshBrowsers();
    m_debugProgram->refreshSessions();
    m_playbackPanel->refresh();
  });
  connect(m_debugProgram, &DebugProgram::replayRequested, this,
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
  // Mirror the concurrent-TX enable state on the capture tab's indicator.
  connect(m_transmitPanel, &TransmitPanel::enabledChanged, m_capturePanel,
          &CapturePanel::setConcurrentTxEnabled);
  m_capturePanel->setConcurrentTxEnabled(m_transmitPanel->transmitEnabled());
  // Live waveform change: swap the running transmission's waveform in place.
  connect(m_transmitPanel, &TransmitPanel::waveformChanged, this, [this] {
    if (m_transmit.running())
      m_transmit.updateWaveform(m_transmitPanel->waveformConfig());
  });

  // Standalone Jam program.
  connect(m_jamProgram, &JamProgram::startRequested, this,
          &MainWindow::startJam);
  connect(m_jamProgram, &JamProgram::stopRequested, this,
          &MainWindow::stopJam);
  connect(&m_jamProcessor, &SpectrumProcessor::spectrumReady, m_jamProgram,
          &JamProgram::pushSpectrum);
  connect(&m_jamMonitor, &CapturePipeline::errorOccurred, this,
          [this](const QString &msg) { statusBar()->showMessage(msg); });

  m_deviceManager.rescan();
  updateCaptureRangeOverlay();
}

MainWindow::~MainWindow() {
  m_capture.stop();
  m_jamMonitor.stop();
  m_transmit.stop();
  m_playback.stop();
}

ProgramScreen *MainWindow::addProgram(const QString &name,
                                      const QString &iconPath,
                                      QWidget *content) {
  auto *screen = new ProgramScreen(name, content, this);
  connect(screen, &ProgramScreen::backRequested, this,
          [this] { m_stack->setCurrentIndex(0); });
  // Tapping either device badge opens the selector, focused on that section.
  connect(screen, &ProgramScreen::txClicked, this,
          [this] { openDevices(DeviceFocus::Tx); });
  connect(screen, &ProgramScreen::rxClicked, this,
          [this] { openDevices(DeviceFocus::Rx); });
  m_programScreens.push_back(screen);
  m_stack->addWidget(screen);
  m_home->addProgram(name, iconPath);
  return screen;
}

void MainWindow::openDevices(DeviceFocus focus) {
  if (m_stack->currentIndex() == m_devicesIndex)
    return; // already selecting devices
  m_devicesReturnIndex = m_stack->currentIndex();
  if (focus == DeviceFocus::Rx)
    m_devicesProgram->focusReceiver();
  else if (focus == DeviceFocus::Tx)
    m_devicesProgram->focusTransmitter();
  m_stack->setCurrentIndex(m_devicesIndex);
}

void MainWindow::openSessions(const QString &sessionDir) {
  if (m_stack->currentIndex() == m_sessionsIndex)
    return; // already in the manager
  m_sessionsReturnIndex = m_stack->currentIndex();
  m_sessionsProgram->refresh();
  if (sessionDir.isEmpty())
    m_sessionsProgram->showList();
  else
    m_sessionsProgram->openSession(sessionDir);
  m_stack->setCurrentIndex(m_sessionsIndex);
}

void MainWindow::refreshDeviceBadges() {
  const auto rx = m_devicesProgram->selectedRx();
  const auto tx = m_devicesProgram->selectedTx();
  const QString rxName = rx ? rx->displayName() : QString();
  const QString txName = tx ? tx->displayName() : QString();
  m_home->setRxName(rxName);
  m_home->setTxName(txName);
  for (ProgramScreen *screen : m_programScreens) {
    screen->setRxName(rxName);
    screen->setTxName(txName);
  }
}

Session *MainWindow::ensureSession() {
  if (!m_session) {
    m_session = m_store.createSession();
    refreshBrowsers();
  }
  return m_session.get();
}

void MainWindow::setActiveCapture(ICaptureControls *controls, bool allowTx) {
  m_activeCapture = controls;
  m_activeAllowTx = allowTx;
}

void MainWindow::refreshBrowsers() {
  m_fobSessionList->refresh();
  if (m_captureSessionList)
    m_captureSessionList->refresh();
  if (m_sessionsProgram)
    m_sessionsProgram->refresh();
}

void MainWindow::startCapture(bool record) {
  const auto rxInfo = m_devicesProgram->selectedRx();
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
      record && m_activeCapture->trigger() == QLatin1String("auto");

  // Every record run gets its own fresh session so recordings from separate
  // runs are never mixed into the same session. Monitor does not record and
  // keeps whatever session is current.
  if (record) {
    m_session = m_store.createSession();
    refreshBrowsers();
    m_playbackPanel->refresh();
  }

  CapturePipeline::Plan plan;
  plan.params = m_activeCapture->streamParams();
  plan.trigger = m_activeCapture->trigger();
  plan.captureRangeHz = m_activeCapture->captureRangeHz();
  if (autoMode) {
    const double rate = plan.params.sampleRateHz;
    const auto msToSamples = [rate](std::chrono::milliseconds ms) {
      return static_cast<qint64>(rate * ms.count() / 1000.0);
    };
    plan.preRollSamples = msToSamples(kAutoPreRoll);
    plan.minSegmentSamples = msToSamples(kAutoMinSegment);
  }
  if (record && !autoMode) {
    // No duration input: record runs until the user hits STOP.
    plan.outputPath = ensureSession()->nextRecordingPath();
  }

  // Optional concurrent waveform transmission on the same channel; runs for
  // both monitor and record stages and can be toggled live (see
  // onTransmitToggled). Only the FOB program offers concurrent TX.
  if (m_activeAllowTx && m_transmitPanel->transmitEnabled()) {
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
  for (SpectrumWidget *view : m_spectrumViews) {
    view->setAxis(plan.params.frequencyHz, plan.params.sampleRateHz);
    view->setMarkers({});
  }
  for (WaterfallWidget *view : m_waterfallViews)
    view->clear();
  updateCaptureRangeOverlay();
  m_peakDetector.setAxis(plan.params.frequencyHz, plan.params.sampleRateHz);
  m_peakDetector.setThresholdDb(
      static_cast<float>(m_activeCapture->peakThresholdDb()));
  m_peakDetector.reset();
  // Peak markers run for monitor and auto (both keep the live spectrum). The
  // LOOKING glass can suppress them via its checkbox.
  m_peakMarkersEnabled = m_activeCapture->peakDetectionEnabled();
  m_monitorLive = !record || autoMode;
  if (autoMode)
    startAutoCapture();
  // Replay mode (auto record only): count segments so the second one triggers
  // the switch to monitor-and-replay. FOB-only feature.
  m_replayActive =
      record && autoMode && m_activeAllowTx && m_activeCapture->replayMode();
  m_disableTxAfterReplay = m_activeCapture->disableTxAfterCapture();
  m_replaySegmentCount = 0;
  m_replayFirstPath.clear();
  m_activeCapture->setRunning(true);
  statusBar()->showMessage(autoMode ? tr("Auto capture armed…")
                           : record ? tr("Recording…")
                                    : tr("Monitoring…"));
}

void MainWindow::startAutoCapture() {
  m_autoActive = true;
  m_autoPhase = AutoPhase::WaitSignal;
  const StreamParams p = m_activeCapture->streamParams();
  m_autoBandCenterHz = p.frequencyHz;
  m_autoBandHalfHz = m_activeCapture->captureRangeHz();
  m_autoSampleRateHz = p.sampleRateHz;
  m_autoOnThresholdDb = static_cast<float>(m_activeCapture->peakThresholdDb());
  m_autoOffThresholdDb = m_autoOnThresholdDb - kAutoHysteresisDb;
  m_autoHangTime = std::chrono::milliseconds(
      static_cast<qint64>(m_activeCapture->hangTimeMs()));
  ensureSession();
}

void MainWindow::driveAutoCapture(const QVector<float> &row) {
  const auto now = std::chrono::steady_clock::now();
  switch (m_autoPhase) {
  case AutoPhase::WaitSignal:
    // Open on the higher on-threshold so noise near the level does not arm.
    if (m_peakDetector.signalPresent(row, m_autoBandCenterHz, m_autoBandHalfHz,
                                     m_autoOnThresholdDb)) {
      m_capture.beginSegment(ensureSession()->nextRecordingPath());
      m_autoPhase = AutoPhase::Recording;
      m_autoLastSignal = now;
      statusBar()->showMessage(tr("Capturing transmission…"));
      m_toasts->show(tr("Auto capture triggered"));
    }
    break;
  case AutoPhase::Recording:
    // Hold open on the lower off-threshold (hysteresis) and only close
    // after the signal has stayed gone for the full hang time.
    if (m_peakDetector.signalPresent(row, m_autoBandCenterHz, m_autoBandHalfHz,
                                     m_autoOffThresholdDb)) {
      m_autoLastSignal = now;
    } else if (now - m_autoLastSignal >= m_autoHangTime) {
      // Drop the silence held open past the signal (the hang window minus
      // a small post-roll) so the stored file ends near the transmission.
      const auto silence =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              now - m_autoLastSignal);
      const auto drop = silence - std::min(silence, kAutoPostPad);
      const qint64 dropSamples =
          static_cast<qint64>(m_autoSampleRateHz * drop.count() / 1000.0);
      m_capture.endSegment(dropSamples);
      m_autoPhase = AutoPhase::Finalizing;
    }
    break;
  case AutoPhase::Finalizing:
    break; // waiting for the worker to trim + write the segment
  }
}

void MainWindow::onAutoSegmentSaved(const RecordingMetadata &meta) {
  Session *session = ensureSession();
  if (session) {
    session->addRecording(meta);
    cacheRecordingSpectrum(meta);
    refreshBrowsers();
    m_playbackPanel->refresh();
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
  m_toasts->show(tr("Auto capture saved: %1").arg(meta.file));

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

void MainWindow::enterReplayMonitor() {
  m_replayActive = false;

  // Drop the concurrent TX noise (and reflect it in the panel), unless the
  // user asked to keep it running through the replay. Unticking while the
  // capture is live stops the stream via onTransmitToggled; the extra stop()
  // covers the case where it was never enabled.
  if (m_disableTxAfterReplay) {
    if (m_transmitPanel->transmitEnabled())
      m_transmitPanel->setTransmitEnabled(false);
    m_transmit.stop();
  }

  // Leave segment capture: the pipeline keeps receiving, now a plain monitor.
  m_autoActive = false;
  m_autoPhase = AutoPhase::WaitSignal;

  // Replay the first captured signal on the transmitter.
  if (m_replayFirstPath.isEmpty()) {
    statusBar()->showMessage(tr("Replay mode: no signal to replay"));
    return;
  }
  // Let the concurrent TX fully stop and release the device before the replay
  // takes over the transmitter.
  statusBar()->showMessage(tr("Replay mode: stopping TX…"));
  const RecordingMetadata meta = m_replayFirstMeta;
  const QString path = m_replayFirstPath;
  const QString sessionDir = m_session ? m_session->dir() : QString();
  QTimer::singleShot(kReplayTxSettleMs, this, [this, meta, path, sessionDir] {
    m_playbackPanel->select(sessionDir, meta.file);
    startPlayback(path, m_playbackPanel->streamParams(), meta.format,
                  m_playbackPanel->speed(), m_playbackPanel->repeat());
    m_toasts->show(tr("Replaying first capture: %1").arg(meta.file));
  });
}

void MainWindow::rearmAuto() {
  // Re-arm for the next transmission; a segment finalized during stop
  // (m_autoActive already false) does not re-arm.
  if (m_autoActive)
    m_autoPhase = AutoPhase::WaitSignal;
}

bool MainWindow::startConcurrentTx(const StreamParams &captureParams) {
  const auto txInfo = m_devicesProgram->selectedTx();
  if (!txInfo) {
    statusBar()->showMessage(
        tr("Concurrent TX enabled but no transmitter selected"));
    return false;
  }
  auto txDevice = m_deviceManager.open(*txInfo);
  if (!txDevice) {
    statusBar()->showMessage(tr("Cannot open %1").arg(txInfo->displayName()));
    return false;
  }
  StreamParams txParams = captureParams;
  txParams.gainDb = m_transmitPanel->txGainDb();
  const WaveformConfig waveform = m_transmitPanel->waveformConfig();
  return m_transmit.start(std::move(txDevice), txParams, waveform);
}

void MainWindow::onTransmitToggled(bool enabled) {
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

void MainWindow::startJam() {
  const auto txInfo = m_devicesProgram->selectedTx();
  if (!txInfo) {
    statusBar()->showMessage(tr("Select a transmitter first"));
    m_jamProgram->setRunning(false);
    return;
  }
  auto txDevice = m_deviceManager.open(*txInfo);
  if (!txDevice) {
    statusBar()->showMessage(tr("Cannot open %1").arg(txInfo->displayName()));
    m_jamProgram->setRunning(false);
    return;
  }

  const StreamParams params = m_jamProgram->streamParams();
  if (!m_transmit.start(std::move(txDevice), params,
                        m_jamProgram->waveformConfig())) {
    m_jamProgram->setRunning(false);
    return;
  }

  // Optionally monitor the channel on the receiver so the FFT/waterfall show
  // the transmission live; the overlay stays even when no receiver is set.
  if (const auto rxInfo = m_devicesProgram->selectedRx()) {
    if (auto rxDevice = m_deviceManager.open(*rxInfo)) {
      CapturePipeline::Plan plan;
      plan.params = params; // monitor only (no output path)
      m_jamMonitor.start(std::move(rxDevice), plan);
    }
  }

  m_jamActive = true;
  m_jamProgram->setRunning(true);
  statusBar()->showMessage(tr("Transmitting %1 MHz")
                               .arg(params.frequencyHz / 1e6, 0, 'f', 3));
}

void MainWindow::stopJam() {
  m_transmit.stop();
  m_jamMonitor.stop();
  m_jamActive = false;
  m_jamProgram->setRunning(false);
  statusBar()->showMessage(tr("TX stopped"));
}

void MainWindow::stopCapture() {
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
  for (SpectrumWidget *view : m_spectrumViews)
    view->setMarkers({});
  if (m_activeCapture)
    m_activeCapture->setRunning(false);
  updateCaptureRangeOverlay();
  statusBar()->showMessage(tr("Stopped"));
}

void MainWindow::updateCaptureRangeOverlay() {
  if (m_capture.running()) {
    // Live: every view shares the running capture's axis (set at start) and
    // band, so both programs' spectra delimit the same captured range.
    const double centerHz = m_activeCaptureParams.frequencyHz;
    const double widthHz =
        2.0 * (m_activeCapture ? m_activeCapture->captureRangeHz() : 0.0);
    for (SpectrumWidget *view : m_spectrumViews)
      view->setCaptureRange(centerHz, widthHz);
    return;
  }
  // Idle: each program previews its own panel's tuning; the spectrum has no
  // axis yet so drive it from the panel and mark ± the capture range.
  const auto apply = [](SpectrumWidget *view, ICaptureControls *ctrl) {
    const StreamParams p = ctrl->streamParams();
    view->setAxis(p.frequencyHz, p.sampleRateHz);
    view->setCaptureRange(p.frequencyHz, 2.0 * ctrl->captureRangeHz());
  };
  apply(m_fobProgram->spectrumView(), m_capturePanel);
  apply(m_captureProgram->spectrumView(), m_captureProgram->capturePanel());
  apply(m_lookingProgram->spectrumView(), m_lookingPanel);
}

void MainWindow::onCaptureFinished(const RecordingMetadata &meta,
                                   bool wasRecording) {
  m_transmit.stop();
  m_monitorLive = false;
  if (m_activeCapture)
    m_activeCapture->setRunning(false);
  if (!wasRecording) {
    statusBar()->showMessage(tr("Monitor stopped"));
    return;
  }
  if (Session *session = ensureSession()) {
    session->addRecording(meta);
    cacheRecordingSpectrum(meta);
    refreshBrowsers();
    m_playbackPanel->refresh();
    statusBar()->showMessage(
        tr("Saved %1 (%2 s)").arg(meta.file).arg(meta.durationSec, 0, 'f', 1));
  }
}

void MainWindow::cacheRecordingSpectrum(const RecordingMetadata &meta) {
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
                               double speed, bool repeat) {
  const auto txInfo = m_devicesProgram->selectedTx();
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
    if (m_activePlayback)
      m_activePlayback->setPlaying(true);
    statusBar()->showMessage(tr("Transmitting %1").arg(absPath));
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // Fixed-size popout: no geometry/state to persist.
  m_capture.stop();
  m_jamMonitor.stop();
  m_transmit.stop();
  m_playback.stop();
  QMainWindow::closeEvent(event);
}

} // namespace duality
