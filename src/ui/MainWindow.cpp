
#include "MainWindow.h"
#include <QApplication>
#include <QDebug>
#include <QCloseEvent>
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
  setFixedSize(1280, 800);

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
  rxFreq->setRange(0.1, 6000);
  txFreq->setRange(0.1, 6000);
  rxFreq->setValue(433.81);
  txFreq->setValue(433.95);

  exitButton = new QPushButton("EXIT", this);
  exitButton->setObjectName("ExitButton");

  startButton = new QPushButton("START", this);
  unlockButton = new QPushButton("UNLOCK", this);
  unlockButton->setEnabled(false);

  captureStatus1 = new QLabel("Capture 1: EMPTY", this);
  captureStatus2 = new QLabel("Capture 2: EMPTY", this);

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
  topBarLayout->setContentsMargins(10, 4, 10, 4);
  topBarLayout->setSpacing(12);
  QLabel *titleLabel = new QLabel("Duality RF Console", topBarWidget);
  titleLabel->setStyleSheet("font-weight: bold;");
  topBarLayout->addWidget(titleLabel);
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
  // Threshold control just above start
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
  // Detector mode dropdown
  thLayout->addSpacing(16);
  thLayout->addWidget(new QLabel("Detector:", this));
  detectorModeCombo = new QComboBox(this);
  detectorModeCombo->addItem("Averaged"); // index 0
  detectorModeCombo->addItem("Peak");     // index 1
  detectorModeCombo->setCurrentIndex(0);
  detectorModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  detectorModeCombo->setMinimumContentsLength(6);
  detectorModeCombo->setEditable(false);
  thLayout->addWidget(detectorModeCombo);
  thLayout->addSpacing(16);
  thLayout->addWidget(triggerStatusLabel);
  layout->addLayout(thLayout);
  layout->addWidget(startButton);
  layout->addWidget(captureStatus1);
  layout->addWidget(captureStatus2);
  layout->addWidget(unlockButton);
  central->setLayout(layout);

  receiver = new SDRReceiver(this);
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  spectrum->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);

  connect(receiver, &SDRReceiver::newFFTData, waterfall,
          &WaterfallWidget::pushData, Qt::QueuedConnection);
  connect(receiver, &SDRReceiver::newFFTData, spectrum,
          &SpectrumWidget::pushData, Qt::QueuedConnection);
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
          });
  connect(zoomSlider, &QSlider::valueChanged, this,
          &MainWindow::onZoomSliderChanged);
  connect(zoomOutButton, &QPushButton::clicked, this, &MainWindow::onZoomOut);
  connect(zoomInButton, &QPushButton::clicked, this, &MainWindow::onZoomIn);
  connect(gainSlider, &QSlider::valueChanged, this, &MainWindow::onGainChanged);
  connect(thresholdSlider, &QSlider::valueChanged, this, &MainWindow::onThresholdChanged);
  connect(sampleRateCombo, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &MainWindow::onSampleRateChanged);
  connect(spanSlider, &QSlider::valueChanged, this, &MainWindow::onSpanChanged);
  connect(detectorModeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &MainWindow::onDetectorModeChanged);

  // initialize threshold in receiver
  if (receiver)
    receiver->setTriggerThresholdDb(-40.0);
  // Initialize span (kHz -> Hz)
  if (receiver)
    receiver->setCaptureSpanHz(spanSlider->value() * 1000.0);
  waterfall->setCaptureSpanHz(spanSlider->value() * 1000.0);
  if (receiver)
    receiver->setDetectorMode(detectorModeCombo->currentIndex());
  qInfo() << "[UI] Initialized with RX(MHz)=" << rxFreq->value()
          << "TX(MHz)=" << txFreq->value()
          << "SR(Hz)=" << sampleRateHz;
  connect(receiver, &SDRReceiver::captureCompleted, this, &MainWindow::onCaptureCompleted);
  connect(receiver, &SDRReceiver::triggerStatus, this, &MainWindow::onTriggerStatus);
}

void MainWindow::startWaterfall() {
  waterfall->reset();
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);
  spectrum->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
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
    running = true;
    startButton->setText("STOP");
    unlockButton->setEnabled(false);
    triggerStatusLabel->setText("Status: Armed");
    triggerStatusLabel->setStyleSheet("");
    if (receiver)
      receiver->armTriggeredCapture(1.0, 0.2);
  } else {
    qInfo() << "[UI] STOP clicked -> Cancel capture";
    // Cancel armed/capturing state
    running = false;
    startButton->setText("START");
    if (receiver)
      receiver->cancelTriggeredCapture();
    captureStatus1->setText("Capture 1: EMPTY");
    captureStatus2->setText("Capture 2: EMPTY");
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
  qInfo() << "[UI] Capture span set to ±" << kHz << "kHz";
}

void MainWindow::onCaptureCompleted(const QString &filePath) {
  Q_UNUSED(filePath);
  running = false;
  startButton->setText("START");
  captureStatus1->setText("Capture 1: CAPTURED");
  // leave capture 2 as EMPTY for now
  unlockButton->setEnabled(true);
  triggerStatusLabel->setText("Status: Captured");
  triggerStatusLabel->setStyleSheet("");
  qInfo() << "[UI] Capture completed ->" << filePath;
}

void MainWindow::onTriggerStatus(bool armed, bool capturing, double centerDb, double thresholdDb, bool above) {
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
  qInfo() << "[UI] Trigger status:" << state
          << "center(dB)=" << centerDb
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

void MainWindow::onRxFrequencyChanged(double frequencyMHz) {
  waterfall->setFrequencyInfo(frequencyMHz * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(frequencyMHz * 1e6, txFreq->value() * 1e6);
  spectrum->setFrequencyInfo(frequencyMHz * 1e6, sampleRateHz);
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
  waterfallActive = false;
  waterfall->reset();

  QMainWindow::closeEvent(event);
}
