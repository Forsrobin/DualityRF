
#include "MainWindow.h"
#include "../core/SDRTransmitter.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <qdatetime.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), running(false), waterfallActive(false),
      sampleRateHz(2.6e6) {
  setWindowTitle("Duality RF Console");
  setFixedSize(1280, 1000);

  QWidget *central = new QWidget(this);
  setCentralWidget(central);

  setStyleSheet(R"(
        QWidget {
            background-color: black;
            color: cyan;
            font-family: monospace;
        }
        QComboBox {
            background-color: #001010;
            color: cyan;
            border: 1px solid cyan;
            padding: 4px 28px 4px 6px; /* room for arrow */
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid cyan;
            background-color: #002222;
        }
        QComboBox::down-arrow {
            width: 0; height: 0;
            border-left: 6px solid transparent;
            border-right: 6px solid transparent;
            border-top: 8px solid cyan;
            margin: 0 auto;
        }
        QComboBox QAbstractItemView {
            background-color: #001010;
            color: cyan;
            selection-background-color: #003333;
            selection-color: cyan;
            border: 1px solid cyan;
        }
        QDoubleSpinBox {
            background-color: #001010;
            color: cyan;
            border: 1px solid cyan;
            padding: 4px 28px 4px 6px;
        }
        QDoubleSpinBox::up-button,
        QDoubleSpinBox::down-button {
            width: 24px;
            border: 1px solid cyan;
            background-color: #002222;
            subcontrol-origin: border;
        }
        QDoubleSpinBox::up-button {
            subcontrol-position: top right;
        }
        QDoubleSpinBox::down-button {
            subcontrol-position: bottom right;
        }
        QDoubleSpinBox::up-button:hover,
        QDoubleSpinBox::down-button:hover {
            background-color: #003333;
        }
        QDoubleSpinBox::up-button:pressed,
        QDoubleSpinBox::down-button:pressed {
            background-color: #004444;
        }
        QDoubleSpinBox::up-arrow {
            width: 0;
            height: 0;
            border-left: 6px solid transparent;
            border-right: 6px solid transparent;
            border-bottom: 8px solid cyan;
            margin: 0 auto;
        }
        QDoubleSpinBox::down-arrow {
            width: 0;
            height: 0;
            border-left: 6px solid transparent;
            border-right: 6px solid transparent;
            border-top: 8px solid cyan;
            margin: 0 auto;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #004040;
            border: 1px solid cyan;
            margin: 4px 0;
        }
        QSlider::handle:horizontal {
            background: cyan;
            width: 12px;
            margin: -6px 0;
        }
        QSlider::add-page:horizontal,
        QSlider::sub-page:horizontal {
            background: #002020;
        }
        QPushButton {
            background-color: #001010;
            color: cyan;
            border: 1px solid cyan;
            padding: 4px 12px;
        }
        QPushButton:disabled {
            background-color: #000808;
            color: #004040;
            border: 1px solid #003030;
        }
        QPushButton:hover {
            background-color: #002020;
        }
        QPushButton:pressed {
            background-color: #003030;
        }
        QPushButton#ExitButton {
            background-color: #280000;
            color: #ff6060;
            border: 1px solid #ff6060;
            padding: 4px 16px;
        }
        QPushButton#ExitButton:disabled {
            background-color: #1a0000;
            color: #5a2b2b;
            border: 1px solid #5a2b2b;
        }
        QFrame#CaptureBox {
            border: 1px solid cyan;
            background-color: #000A0A;
        }
        QFrame#CaptureBox[completed="true"] {
            border: 2px solid #00ff80; /* thicker green when complete */
        }
        QFrame#CaptureBox > QLabel {
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#ExitButton:hover {
            background-color: #3a0000;
            color: #ff8080;
        }
        QPushButton#ExitButton:pressed {
            background-color: #520000;
        }
        QLabel {
            color: cyan;
        }
        #TopBar {
            background-color: #001818;
            border-bottom: 1px solid cyan;
        }
    )");

  spectrum = new SpectrumWidget(this);
  waterfall = new WaterfallWidget(this);

  rxFreq = new QDoubleSpinBox(this);
  txFreq = new QDoubleSpinBox(this);
  rxFreq->setSuffix(" MHz");
  txFreq->setSuffix(" MHz");
  // Step size: 1 kHz = 0.001 MHz
  rxFreq->setDecimals(3);
  txFreq->setDecimals(3);
  rxFreq->setSingleStep(0.001);
  txFreq->setSingleStep(0.001);
  rxFreq->setRange(0.1, 6000);
  txFreq->setRange(0.1, 6000);
  rxFreq->setValue(433.81);
  txFreq->setValue(434.20);

  exitButton = new QPushButton("EXIT", this);
  exitButton->setObjectName("ExitButton");

  startButton = new QPushButton("START", this);
  unlockButton = new QPushButton("UNLOCK", this);
  unlockButton->setEnabled(false);

  captureStatus1 = new QLabel("Capture 1: EMPTY", this); // legacy label
  captureStatus2 = new QLabel("Capture 2: EMPTY", this); // legacy label
  captureStatus1->hide();
  captureStatus2->hide();
  captureBox1 = new CapturePreviewWidget("Capture 1", this);
  captureBox2 = new CapturePreviewWidget("Capture 2", this);

  // Threshold slider (-100..0 dB), default -30 dB
  thresholdSlider = new QSlider(Qt::Horizontal, this);
  thresholdSlider->setRange(0, 100);
  thresholdSlider->setSingleStep(1);
  thresholdSlider->setPageStep(5);
  // default to -40 dB (more sensitive; typical peaks ~-38 dB)
  thresholdSlider->setValue(60); // maps to -40 dB
  thresholdSlider->setFocusPolicy(Qt::NoFocus);
  thresholdLabel = new QLabel("Threshold: -40 dB", this);
  triggerStatusLabel = new QLabel("Status: Idle", this);

  zoomOutButton = new QPushButton("-", this);
  zoomInButton = new QPushButton("+", this);
  zoomOutButton->setFixedWidth(36);
  zoomInButton->setFixedWidth(36);

  const int initialZoomStep = kZoomMinStep;

  zoomSlider = new QSlider(Qt::Horizontal, this);
  zoomSlider->setRange(kZoomMinStep, kZoomMaxStep);
  zoomSlider->setSingleStep(1);
  zoomSlider->setPageStep(1);
  zoomSlider->setValue(initialZoomStep);
  zoomSlider->setFocusPolicy(Qt::NoFocus);
  zoomSlider->installEventFilter(this);

  zoomLabel = new QLabel(QString("Zoom: %1x").arg(1 << initialZoomStep), this);
  waterfall->setZoomStep(initialZoomStep);
  spectrum->setZoomStep(initialZoomStep);

  // Gain slider (maps to known RTL-SDR gains)
  gainSlider = new QSlider(Qt::Horizontal, this);
  gainSlider->setRange(0, 28); // 29 supported gains
  gainSlider->setSingleStep(1);
  gainSlider->setPageStep(1);
  gainSlider->setValue(22); // ~40.2 dB default
  gainSlider->setFocusPolicy(Qt::NoFocus);
  gainLabel = new QLabel("Gain: 40.2 dB", this);

  // Sample rate combo
  sampleRateCombo = new QComboBox(this);
  const QList<int> rates = {250000,  1024000, 1200000, 1440000, 1536000,
                            1600000, 1800000, 1920000, 2048000, 2200000,
                            2400000, 2560000, 2800000, 3000000, 3200000};
  for (int r : rates)
    sampleRateCombo->addItem(QString::number(r), r);
  // select nearest to current
  int best = 0;
  int bestDiff = INT_MAX;
  for (int i = 0; i < sampleRateCombo->count(); ++i) {
    int r = sampleRateCombo->itemData(i).toInt();
    int diff = std::abs(r - int(sampleRateHz));
    if (diff < bestDiff) {
      best = i;
      bestDiff = diff;
    }
  }
  sampleRateCombo->setCurrentIndex(best);
  // Ensure contents fit inside the combobox
  sampleRateCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  sampleRateCombo->setMinimumContentsLength(9);
  sampleRateCombo->setEditable(false);

  QWidget *topBarWidget = new QWidget(this);
  topBarWidget->setObjectName("TopBar");
  topBarWidget->setFixedHeight(30);
  topBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QHBoxLayout *topBarLayout = new QHBoxLayout(topBarWidget);
  topBarLayout->setContentsMargins(10, 6, 10, 6);
  topBarLayout->setSpacing(12);
  // Reset Peaks and Info buttons on the far left
  QPushButton *resetPeaksBtn = new QPushButton("RESET PEAKS", topBarWidget);
  resetCapturesButton = new QPushButton("RESET", topBarWidget);
  infoButton = new QPushButton("INFO", topBarWidget);
  topBarLayout->addWidget(resetPeaksBtn);
  topBarLayout->addWidget(resetCapturesButton);
  topBarLayout->addWidget(infoButton);
  topBarLayout->addStretch(1);
  topBarLayout->addWidget(exitButton);

  QHBoxLayout *fftLayout = new QHBoxLayout;
  fftLayout->setSpacing(12);
  QLabel *fftDescriptor = new QLabel("Zoom:", this);
  fftLayout->addWidget(fftDescriptor);
  fftLayout->addWidget(zoomOutButton);
  fftLayout->addWidget(zoomSlider, 1);
  fftLayout->addWidget(zoomInButton);
  fftLayout->addWidget(zoomLabel);

  QHBoxLayout *freqLayout = new QHBoxLayout;
  freqLayout->setSpacing(12);
  freqLayout->addWidget(new QLabel("TX Frequency:", this));
  freqLayout->addWidget(txFreq);
  freqLayout->addSpacing(20);
  freqLayout->addWidget(new QLabel("RX Frequency:", this));
  freqLayout->addWidget(rxFreq);

  QFrame *line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  QVBoxLayout *layout = new QVBoxLayout(central);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);
  layout->addWidget(topBarWidget);
  layout->addWidget(spectrum);
  layout->addWidget(waterfall, 1);
  layout->addWidget(line);
  layout->addLayout(fftLayout);
  // Gain & sample rate controls
  QHBoxLayout *gainRateLayout = new QHBoxLayout;
  gainRateLayout->setSpacing(12);
  gainRateLayout->addWidget(new QLabel("Sample Rate:", this));
  gainRateLayout->addWidget(sampleRateCombo);
  gainRateLayout->addSpacing(20);
  gainRateLayout->addWidget(new QLabel("Gain:", this));
  gainRateLayout->addWidget(gainSlider, 1);
  gainRateLayout->addWidget(gainLabel);
  layout->addLayout(gainRateLayout);
  layout->addLayout(freqLayout);

  // Detector + dwell/avg tau row under RX/TX inputs
  QHBoxLayout *detLayout = new QHBoxLayout;
  detLayout->setSpacing(12);
  detLayout->addWidget(new QLabel("Detector:", this));
  detectorModeCombo = new QComboBox(this);
  detectorModeCombo->addItem("Averaged"); // index 0
  detectorModeCombo->addItem("Peak");     // index 1
  detectorModeCombo->setCurrentIndex(0);
  detectorModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  detectorModeCombo->setMinimumContentsLength(6);
  detectorModeCombo->setEditable(false);
  detLayout->addWidget(detectorModeCombo);

  detLayout->addSpacing(16);
  detLayout->addWidget(new QLabel("Dwell:", this));
  dwellSpin = new QDoubleSpinBox(this);
  dwellSpin->setDecimals(3);
  dwellSpin->setRange(0.0, 1.0);
  dwellSpin->setSingleStep(0.01);
  dwellSpin->setValue(0.02); // default 20 ms
  dwellSpin->setSuffix(" s");
  detLayout->addWidget(dwellSpin);

  detLayout->addSpacing(12);
  detLayout->addWidget(new QLabel("Avg Tau:", this));
  avgTauSpin = new QDoubleSpinBox(this);
  avgTauSpin->setDecimals(3);
  avgTauSpin->setRange(0.0, 2.0);
  avgTauSpin->setSingleStep(0.05);
  avgTauSpin->setValue(0.20); // default 200 ms
  avgTauSpin->setSuffix(" s");
  detLayout->addWidget(avgTauSpin);

  layout->addLayout(detLayout);

  // Trigger status label above trigger controls
  // Threshold + capture span row
  QHBoxLayout *thLayout = new QHBoxLayout;
  thLayout->setSpacing(12);
  thLayout->addWidget(new QLabel("Capture Threshold:", this));
  thLayout->addWidget(thresholdSlider, 1);
  thLayout->addWidget(thresholdLabel);
  // Span slider: ±Hz around RX used for detection and visualization
  spanSlider = new QSlider(Qt::Horizontal, this);
  spanSlider->setRange(1, 400); // 1..400 kHz
  spanSlider->setSingleStep(1);
  spanSlider->setPageStep(10);
  spanSlider->setValue(100); // default ±100 kHz
  spanSlider->setFocusPolicy(Qt::NoFocus);
  spanLabel = new QLabel("Span: ±100 kHz", this);
  thLayout->addSpacing(16);
  thLayout->addWidget(new QLabel("Capture Span:", this));
  thLayout->addWidget(spanSlider, 1);
  thLayout->addWidget(spanLabel);
  // Place status text above the control row
  layout->addWidget(triggerStatusLabel);
  layout->addLayout(thLayout);
  // TX noise controls row (under capture controls)
  QHBoxLayout *txNoiseLayout = new QHBoxLayout;
  txNoiseLayout->setSpacing(12);
  noiseIntensitySlider = new QSlider(Qt::Horizontal, this);
  noiseIntensitySlider->setRange(0, 47); // TX VGA 0..47 dB
  noiseIntensitySlider->setSingleStep(1);
  noiseIntensitySlider->setPageStep(5);
  noiseIntensitySlider->setValue(25); // default 25 dB
  noiseIntensitySlider->setFocusPolicy(Qt::NoFocus);
  noiseIntensityLabel = new QLabel("TX Gain: 25 dB", this);
  txNoiseLayout->addWidget(new QLabel("TX Gain:", this));
  txNoiseLayout->addWidget(noiseIntensitySlider, 1);
  txNoiseLayout->addWidget(noiseIntensityLabel);
  noiseSpanSlider = new QSlider(Qt::Horizontal, this);
  noiseSpanSlider->setRange(1, 400); // 1..400 kHz baseband half-span
  noiseSpanSlider->setSingleStep(1);
  noiseSpanSlider->setPageStep(10);
  noiseSpanSlider->setValue(100);
  noiseSpanSlider->setFocusPolicy(Qt::NoFocus);
  noiseSpanLabel = new QLabel("Noise Span: ±100 kHz", this);
  txNoiseLayout->addSpacing(16);
  txNoiseLayout->addWidget(new QLabel("Noise Span:", this));
  txNoiseLayout->addWidget(noiseSpanSlider, 1);
  txNoiseLayout->addWidget(noiseSpanLabel);
  layout->addLayout(txNoiseLayout);
  layout->addWidget(startButton);
  // Two capture boxes in one row
  {
    QHBoxLayout *caps = new QHBoxLayout;
    caps->setSpacing(12);
    caps->addWidget(captureBox1, 1);
    caps->addWidget(captureBox2, 1);
    layout->addLayout(caps);
  }
  layout->addWidget(unlockButton);
  central->setLayout(layout);

  receiver = new SDRReceiver(this);
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  spectrum->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);
  spectrum->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);
  // Configure capture preview boxes
  captureBox1->setFrequencyInfo(rxFreq->value() * 1e6, rxFreq->value() * 1e6,
                                sampleRateHz);
  captureBox1->setCaptureSpanHz(spanSlider->value() * 1000.0);
  captureBox2->setFrequencyInfo(rxFreq->value() * 1e6, rxFreq->value() * 1e6,
                                sampleRateHz);
  captureBox2->setCaptureSpanHz(spanSlider->value() * 1000.0);

  connect(receiver, &SDRReceiver::newFFTData, waterfall,
          &WaterfallWidget::pushData, Qt::QueuedConnection);
  connect(receiver, &SDRReceiver::newFFTData, spectrum,
          &SpectrumWidget::pushData, Qt::QueuedConnection);
  connect(resetPeaksBtn, &QPushButton::clicked, spectrum,
          &SpectrumWidget::resetPeaks);
  connect(resetCapturesButton, &QPushButton::clicked, this,
          &MainWindow::onResetCaptures);
  connect(infoButton, &QPushButton::clicked, this, [this]() {
    if (!infoDialog)
      infoDialog = new InfoDialog(this);
    infoDialog->show();
    infoDialog->raise();
    infoDialog->activateWindow();
  });
  connect(startButton, &QPushButton::clicked, this, &MainWindow::onStart);
  connect(unlockButton, &QPushButton::clicked, this,
          &MainWindow::onStateUpdate);
  connect(exitButton, &QPushButton::clicked, this,
          &MainWindow::onExitRequested);
  connect(rxFreq,
          static_cast<void (QDoubleSpinBox::*)(double)>(
              &QDoubleSpinBox::valueChanged),
          this, &MainWindow::onRxFrequencyChanged);
  connect(txFreq,
          static_cast<void (QDoubleSpinBox::*)(double)>(
              &QDoubleSpinBox::valueChanged),
          this, [this](double txMHz) {
            waterfall->setRxTxFrequencies(rxFreq->value() * 1e6, txMHz * 1e6);
            spectrum->setRxTxFrequencies(rxFreq->value() * 1e6, txMHz * 1e6);
            if (transmitter)
              transmitter->setFrequencyMHz(txMHz);
          });
  connect(zoomSlider, &QSlider::valueChanged, this,
          &MainWindow::onZoomSliderChanged);
  connect(zoomOutButton, &QPushButton::clicked, this, &MainWindow::onZoomOut);
  connect(zoomInButton, &QPushButton::clicked, this, &MainWindow::onZoomIn);
  connect(gainSlider, &QSlider::valueChanged, this, &MainWindow::onGainChanged);
  connect(thresholdSlider, &QSlider::valueChanged, this,
          &MainWindow::onThresholdChanged);
  connect(sampleRateCombo, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &MainWindow::onSampleRateChanged);
  connect(spanSlider, &QSlider::valueChanged, this, &MainWindow::onSpanChanged);
  connect(noiseIntensitySlider, &QSlider::valueChanged, this,
          &MainWindow::onNoiseIntensityChanged);
  connect(noiseSpanSlider, &QSlider::valueChanged, this,
          &MainWindow::onNoiseSpanChanged);
  connect(detectorModeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &MainWindow::onDetectorModeChanged);
  connect(dwellSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onDwellChanged);
  connect(avgTauSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &MainWindow::onAvgTauChanged);

  // initialize threshold in receiver
  if (receiver)
    receiver->setTriggerThresholdDb(-30.0);
  spectrum->setThresholdDb(-30.0);
  // Initialize span (kHz -> Hz)
  if (receiver)
    receiver->setCaptureSpanHz(spanSlider->value() * 1000.0);
  waterfall->setCaptureSpanHz(spanSlider->value() * 1000.0);
  spectrum->setCaptureSpanHz(spanSlider->value() * 1000.0);
  captureBox1->setCaptureSpanHz(spanSlider->value() * 1000.0);
  captureBox2->setCaptureSpanHz(spanSlider->value() * 1000.0);
  if (receiver)
    receiver->setDetectorMode(detectorModeCombo->currentIndex());
  if (receiver) {
    receiver->setDwellSeconds(0.02);
    receiver->setAvgTauSeconds(0.20);
  }
  // Initialize transmitter (HackRF TX)
  transmitter = new SDRTransmitter(this);
  transmitter->setSampleRate(sampleRateHz);
  transmitter->setFrequencyMHz(txFreq->value());
  transmitter->setTxGainDb(noiseIntensitySlider->value());
  transmitter->setNoiseSpanHz(noiseSpanSlider->value() * 1000.0);
  // Initialize visual noise span overlays
  waterfall->setNoiseSpanHz(noiseSpanSlider->value() * 1000.0);
  spectrum->setNoiseSpanHz(noiseSpanSlider->value() * 1000.0);
  qInfo() << "[UI] Initialized with RX(MHz)=" << rxFreq->value()
          << "TX(MHz)=" << txFreq->value() << "SR(Hz)=" << sampleRateHz;
  connect(receiver, &SDRReceiver::captureCompleted, this,
          &MainWindow::onCaptureCompleted);
  connect(receiver, &SDRReceiver::triggerStatus, this,
          &MainWindow::onTriggerStatus);

  // Clear any old captures at program start
  {
    QDir capDir("captures");
    if (capDir.exists()) {
      capDir.removeRecursively();
    }
    QDir().mkpath("captures");
    qInfo() << "[UI] Cleared previous captures folder";
  }
}

void MainWindow::startWaterfall() {
  waterfall->reset();
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);
  spectrum->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  spectrum->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);
  captureBox1->setFrequencyInfo(rxFreq->value() * 1e6, rxFreq->value() * 1e6,
                                sampleRateHz);
  captureBox2->setFrequencyInfo(rxFreq->value() * 1e6, rxFreq->value() * 1e6,
                                sampleRateHz);
  receiver->startStream(rxFreq->value(), sampleRateHz);
  waterfallActive = true;
  qInfo() << "[UI] Waterfall started";
}

void MainWindow::onStart() {
  if (!running) {
    qInfo() << "[UI] START clicked -> Arm capture";
    // Arm trigger capture: clear statuses and begin buffering
    captureStatus1->setText("Capture 1: EMPTY");
    captureStatus2->setText("Capture 2: EMPTY");
    captureBox1->showEmpty();
    captureBox2->showEmpty();
    captureBox1->setCompleted(false);
    captureBox2->setCompleted(false);
    capture1Done = false;
    capture2Done = false;
    running = true;
    startButton->setText("STOP");
    unlockButton->setEnabled(false);
    triggerStatusLabel->setText("Status: Armed");
    triggerStatusLabel->setStyleSheet("");
    if (receiver)
      receiver->armTriggeredCapture(0.2, 0.2);
    // Start HackRF TX with current settings
    if (transmitter) {
      transmitter->setSampleRate(sampleRateHz);
      transmitter->setFrequencyMHz(txFreq->value());
      transmitter->setTxGainDb(noiseIntensitySlider->value());
      transmitter->setNoiseSpanHz(noiseSpanSlider->value() * 1000.0);
      transmitter->start();
    }
  } else {
    qInfo() << "[UI] STOP clicked -> Cancel capture";
    // Cancel armed/capturing state
    running = false;
    startButton->setText("START");
    if (receiver)
      receiver->cancelTriggeredCapture();
    if (transmitter)
      transmitter->stop();
    captureStatus1->setText("Capture 1: EMPTY");
    captureStatus2->setText("Capture 2: EMPTY");
    captureBox1->showEmpty();
    captureBox2->showEmpty();
    captureBox1->setCompleted(false);
    captureBox2->setCompleted(false);
    capture1Done = false;
    capture2Done = false;
    unlockButton->setEnabled(false);
    triggerStatusLabel->setText("Status: Idle");
    triggerStatusLabel->setStyleSheet("");
  }
}

void MainWindow::onThresholdChanged(int sliderValue) {
  // map 0..100 -> -100..0 dB
  double db = -100.0 + sliderValue;
  thresholdLabel->setText(QString("Threshold: %1 dB").arg(db, 0, 'f', 0));
  if (receiver)
    receiver->setTriggerThresholdDb(db);
  if (spectrum)
    spectrum->setThresholdDb(db);
  qInfo() << "[UI] Threshold set to (dB)=" << db;
}

void MainWindow::onSpanChanged(int sliderValue) {
  // slider in kHz units
  int kHz = std::clamp(sliderValue, 1, 400);
  double halfHz = kHz * 1000.0;
  spanLabel->setText(QString("Span: ±%1 kHz").arg(kHz));
  if (receiver)
    receiver->setCaptureSpanHz(halfHz);
  if (waterfall)
    waterfall->setCaptureSpanHz(halfHz);
  if (spectrum)
    spectrum->setCaptureSpanHz(halfHz);
  if (captureBox1)
    captureBox1->setCaptureSpanHz(halfHz);
  if (captureBox2)
    captureBox2->setCaptureSpanHz(halfHz);
  qInfo() << "[UI] Capture span set to ±" << kHz << "kHz";
}

void MainWindow::onNoiseIntensityChanged(int value) {
  int clamped = std::clamp(value, 0, 47);
  noiseIntensityLabel->setText(QString("TX Gain: %1 dB").arg(clamped));
  if (transmitter)
    transmitter->setTxGainDb(clamped);
  qInfo() << "[UI] TX gain ->" << clamped << "dB";
}

void MainWindow::onNoiseSpanChanged(int kHz) {
  int ck = std::clamp(kHz, 1, 400);
  noiseSpanLabel->setText(QString("Noise Span: ±%1 kHz").arg(ck));
  if (transmitter)
    transmitter->setNoiseSpanHz(ck * 1000.0);
  if (waterfall)
    waterfall->setNoiseSpanHz(ck * 1000.0);
  if (spectrum)
    spectrum->setNoiseSpanHz(ck * 1000.0);
  qInfo() << "[UI] Noise span -> ±" << ck << "kHz";
}

void MainWindow::onCaptureCompleted(const QString &filePath) {
  Q_UNUSED(filePath);
  qInfo() << "[UI] Capture completed ->" << filePath;

  if (!capture1Done) {
    // First capture finished, update status and immediately arm for capture 2
    capture1Done = true;
    captureStatus1->setText("Capture 1: CAPTURED");
    // Compute output sample rate based on current span and decimation logic
    double spanHalfHz = spanSlider->value() * 1000.0;
    int D = std::max(1, int(std::floor(sampleRateHz / (2.0 * spanHalfHz))));
    double outRate = sampleRateHz / double(D);
    captureBox1->setFrequencyInfo(rxFreq->value() * 1e6, rxFreq->value() * 1e6,
                                  outRate);
    captureBox1->loadFromFile(filePath);
    captureBox1->setCompleted(true);
    unlockButton->setEnabled(false);
    triggerStatusLabel->setText("Status: 1/2 captured • Re-armed");
    triggerStatusLabel->setStyleSheet("");
    // Keep running and re-arm for the second capture
    if (receiver && running) {
      receiver->armTriggeredCapture(0.2, 0.2);
    } else if (receiver && !running) {
      // If for some reason running toggled, ensure we continue the sequence
      running = true;
      startButton->setText("STOP");
      receiver->armTriggeredCapture(0.2, 0.2);
    }
    return;
  }

  if (!capture2Done) {
    // Second capture finished
    capture2Done = true;
    captureStatus2->setText("Capture 2: CAPTURED");
    double spanHalfHz = spanSlider->value() * 1000.0;
    int D = std::max(1, int(std::floor(sampleRateHz / (2.0 * spanHalfHz))));
    double outRate = sampleRateHz / double(D);
    captureBox2->setFrequencyInfo(rxFreq->value() * 1e6, rxFreq->value() * 1e6,
                                  outRate);
    captureBox2->loadFromFile(filePath);
    captureBox2->setCompleted(true);
    running = false;
    startButton->setText("START");
    unlockButton->setEnabled(true);
    // Auto-stop TX noise when both captures are done
    if (transmitter)
      transmitter->stop();
    triggerStatusLabel->setText("Status: Both captured");
    triggerStatusLabel->setStyleSheet("");
    return;
  }
}

void MainWindow::onTriggerStatus(bool armed, bool capturing, double centerDb,
                                 double thresholdDb, bool above) {
  Q_UNUSED(capturing);
  if (!armed) {
    triggerStatusLabel->setText("Status: Idle");
    triggerStatusLabel->setStyleSheet("");
    return;
  }
  QString state = above ? "Above" : "Below";
  QString mode = (detectorModeCombo && detectorModeCombo->currentIndex() == 1)
                     ? "Peak"
                     : "Avg";
  triggerStatusLabel->setText(
      QString("Status: Armed • %1 • %2 (Center: %3 dB | Thr: %4 dB)")
          .arg(state)
          .arg(mode)
          .arg(centerDb, 0, 'f', 1)
          .arg(thresholdDb, 0, 'f', 0));
  qInfo() << "[UI] Trigger status:" << state << "center(dB)=" << centerDb
          << "thr(dB)=" << thresholdDb;
  if (above) {
    triggerStatusLabel->setStyleSheet("color: #80ff80;");
  } else {
    triggerStatusLabel->setStyleSheet("color: #ffff80;");
  }
}

void MainWindow::onDetectorModeChanged(int index) {
  Q_UNUSED(index);
  if (receiver)
    receiver->setDetectorMode(detectorModeCombo->currentIndex());
  qInfo() << "[UI] Detector mode ->"
          << (detectorModeCombo->currentIndex() == 1 ? "Peak" : "Averaged");
}

void MainWindow::onDwellChanged(double seconds) {
  if (receiver)
    receiver->setDwellSeconds(std::max(0.0, seconds));
  qInfo() << "[UI] Dwell seconds ->" << seconds;
}

void MainWindow::onAvgTauChanged(double seconds) {
  if (receiver)
    receiver->setAvgTauSeconds(std::max(0.0, seconds));
  qInfo() << "[UI] Avg tau seconds ->" << seconds;
}

void MainWindow::onResetCaptures() {
  qInfo() << "[UI] RESET clicked -> Clear captures & reset state";
  // 1) Stop preview threads first to avoid races with file deletion
  if (captureBox1)
    captureBox1->showEmpty();
  if (captureBox2)
    captureBox2->showEmpty();

  // 2) Stop any armed/capture state and return to idle
  if (receiver) {
    receiver->cancelTriggeredCapture();
  }
  running = false;
  startButton->setText("START");

  // 3) Clear captures folder on disk
  {
    QDir capDir("captures");
    if (capDir.exists())
      capDir.removeRecursively();
    QDir().mkpath("captures");
  }

  // 4) Reset UI state
  capture1Done = false;
  capture2Done = false;
  captureStatus1->setText("Capture 1: EMPTY");
  captureStatus2->setText("Capture 2: EMPTY");
  if (captureBox1)
    captureBox1->setCompleted(false);
  if (captureBox2)
    captureBox2->setCompleted(false);
  unlockButton->setEnabled(false);
  triggerStatusLabel->setText("Status: Idle");
  triggerStatusLabel->setStyleSheet("");
}

void MainWindow::onRxFrequencyChanged(double frequencyMHz) {
  waterfall->setFrequencyInfo(frequencyMHz * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(frequencyMHz * 1e6, txFreq->value() * 1e6);
  spectrum->setFrequencyInfo(frequencyMHz * 1e6, sampleRateHz);
  spectrum->setRxTxFrequencies(frequencyMHz * 1e6, txFreq->value() * 1e6);
  captureBox1->setFrequencyInfo(frequencyMHz * 1e6, frequencyMHz * 1e6,
                                sampleRateHz);
  captureBox2->setFrequencyInfo(frequencyMHz * 1e6, frequencyMHz * 1e6,
                                sampleRateHz);
  if (!waterfallActive)
    return;

  waterfall->reset();
  spectrum->resetPeaks();
  receiver->startStream(frequencyMHz, sampleRateHz);
  qInfo() << "[UI] RX frequency changed ->" << frequencyMHz << "MHz";
}

void MainWindow::onExitRequested() { close(); }

void MainWindow::onZoomSliderChanged(int sliderStep) {
  applyZoomStep(sliderStep);
}

void MainWindow::onZoomOut() {
  int newStep = clampZoomStep(zoomSlider->value() - 1);
  if (newStep != zoomSlider->value())
    zoomSlider->setValue(newStep);
}

void MainWindow::onZoomIn() {
  int newStep = clampZoomStep(zoomSlider->value() + 1);
  if (newStep != zoomSlider->value())
    zoomSlider->setValue(newStep);
}

void MainWindow::onStateUpdate() {
  // Example post-transmit logic
  captureStatus1->setText("Capture 1: TRANSMITTED");
  captureStatus2->setText("Capture 2: CAPTURED");
  unlockButton->setEnabled(true);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == zoomSlider) {
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
      return true;
    default:
      break;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

int MainWindow::clampZoomStep(int step) const {
  return std::clamp(step, kZoomMinStep, kZoomMaxStep);
}

void MainWindow::applyZoomStep(int step) {
  int clamped = clampZoomStep(step);
  int factor = 1 << clamped;
  zoomLabel->setText(QString("Zoom: %1x").arg(factor));
  waterfall->setZoomStep(clamped);
  spectrum->setZoomStep(clamped);
  // Update frequency markers to reflect visible span
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  spectrum->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  if (waterfallActive)
    waterfall->reset();
}

void MainWindow::onGainChanged(int sliderValue) {
  static const double gains[] = {0.0,  0.9,  1.4,  2.7,  3.7,  7.7,  8.7,  12.5,
                                 14.4, 15.7, 16.6, 19.7, 20.7, 22.9, 25.4, 28.0,
                                 29.7, 32.8, 33.8, 36.4, 37.2, 38.6, 40.2, 42.1,
                                 43.4, 43.9, 44.5, 48.0, 49.6};
  int idx =
      std::clamp(sliderValue, 0, int(sizeof(gains) / sizeof(gains[0])) - 1);
  double g = gains[idx];
  gainLabel->setText(QString("Gain: %1 dB").arg(g, 0, 'f', 1));
  if (receiver)
    receiver->setGainDb(g);
  qInfo() << "[UI] Gain changed ->" << g << "dB";
}

void MainWindow::onSampleRateChanged(int index) {
  int sr = sampleRateCombo->itemData(index).toInt();
  if (sr <= 0)
    return;
  sampleRateHz = sr;
  spectrum->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  if (waterfallActive) {
    waterfall->reset();
    spectrum->resetPeaks();
    receiver->startStream(rxFreq->value(), sampleRateHz);
  }
  if (transmitter) {
    transmitter->setSampleRate(sampleRateHz);
  }
  qInfo() << "[UI] Sample rate changed ->" << sr;
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (running) {
    receiver->stopCapture();
    running = false;
    startButton->setText("START");
    captureStatus1->setText("Capture 1: EMPTY");
    captureStatus2->setText("Capture 2: EMPTY");
    unlockButton->setEnabled(false);
  }

  if (receiver) {
    receiver->stopStream();
  }
  if (transmitter) {
    transmitter->stop();
  }
  waterfallActive = false;
  waterfall->reset();

  QMainWindow::closeEvent(event);
}
