
#include "MainWindow.h"
#include <QApplication>
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
  setFixedSize(1280, 720);

  QWidget *central = new QWidget(this);
  setCentralWidget(central);

  setStyleSheet(R"(
        QWidget {
            background-color: black;
            color: cyan;
            font-family: monospace;
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

  fftDecreaseButton = new QPushButton("-", this);
  fftIncreaseButton = new QPushButton("+", this);
  fftDecreaseButton->setFixedWidth(36);
  fftIncreaseButton->setFixedWidth(36);

  const int initialFftValue = 4096;
  const int initialStep = clampFftStep((initialFftValue - kFftMin) / kFftStep);

  fftSizeSlider = new QSlider(Qt::Horizontal, this);
  fftSizeSlider->setRange(0, kFftSteps);
  fftSizeSlider->setSingleStep(1);
  fftSizeSlider->setPageStep(1);
  fftSizeSlider->setValue(initialStep);
  fftSizeSlider->setFocusPolicy(Qt::NoFocus);
  fftSizeSlider->installEventFilter(this);

  fftSizeLabel = new QLabel(QString("FFT: %1").arg(initialFftValue), this);

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
  QLabel *fftDescriptor = new QLabel("Waterfall Resolution:", this);
  fftLayout->addWidget(fftDescriptor);
  fftLayout->addWidget(fftDecreaseButton);
  fftLayout->addWidget(fftSizeSlider, 1);
  fftLayout->addWidget(fftIncreaseButton);
  fftLayout->addWidget(fftSizeLabel);

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
  layout->addWidget(waterfall, 1);
  layout->addWidget(line);
  layout->addLayout(fftLayout);
  layout->addLayout(freqLayout);
  layout->addWidget(startButton);
  layout->addWidget(captureStatus1);
  layout->addWidget(captureStatus2);
  layout->addWidget(unlockButton);
  central->setLayout(layout);

  receiver = new SDRReceiver(this);
  receiver->setFftSize(initialFftValue);
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);

  connect(receiver, &SDRReceiver::newFFTData, waterfall,
          &WaterfallWidget::pushData, Qt::QueuedConnection);
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
            waterfall->setRxTxFrequencies(rxFreq->value() * 1e6,
                                          txMHz * 1e6);
          });
  connect(fftSizeSlider, &QSlider::valueChanged, this,
          &MainWindow::onFftSizeSliderChanged);
  connect(fftDecreaseButton, &QPushButton::clicked, this,
          &MainWindow::onDecreaseFft);
  connect(fftIncreaseButton, &QPushButton::clicked, this,
          &MainWindow::onIncreaseFft);
}

void MainWindow::startWaterfall() {
  waterfall->reset();
  waterfall->setFrequencyInfo(rxFreq->value() * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(rxFreq->value() * 1e6, txFreq->value() * 1e6);
  receiver->startStream(rxFreq->value(), sampleRateHz);
  waterfallActive = true;
}

void MainWindow::onStart() {
  // capture toggle only
  if (!running) {
    running = true;
    startButton->setText("STOP");
    // pick a simple path
    QDir().mkpath("captures");
    const QString path =
        QString("captures/%1.cf32")
            .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss"));
    receiver->startCapture(path);
    captureStatus1->setText("Capture 1: CAPTURED");
    captureStatus2->setText("Capture 2: TRANSMITTED");
    unlockButton->setEnabled(true);
  } else {
    running = false;
    startButton->setText("START");
    receiver->stopCapture();
    captureStatus1->setText("Capture 1: EMPTY");
    captureStatus2->setText("Capture 2: EMPTY");
    unlockButton->setEnabled(false);
  }
}

void MainWindow::onRxFrequencyChanged(double frequencyMHz) {
  waterfall->setFrequencyInfo(frequencyMHz * 1e6, sampleRateHz);
  waterfall->setRxTxFrequencies(frequencyMHz * 1e6, txFreq->value() * 1e6);
  if (!waterfallActive)
    return;

  waterfall->reset();
  receiver->startStream(frequencyMHz, sampleRateHz);
}

void MainWindow::onExitRequested() { close(); }

void MainWindow::onFftSizeSliderChanged(int sliderStep) {
  applyFftSize(fftStepToValue(sliderStep));
}

void MainWindow::onDecreaseFft() {
  int newStep = clampFftStep(fftSizeSlider->value() - 1);
  if (newStep != fftSizeSlider->value())
    fftSizeSlider->setValue(newStep);
}

void MainWindow::onIncreaseFft() {
  int newStep = clampFftStep(fftSizeSlider->value() + 1);
  if (newStep != fftSizeSlider->value())
    fftSizeSlider->setValue(newStep);
}

void MainWindow::onStateUpdate() {
  // Example post-transmit logic
  captureStatus1->setText("Capture 1: TRANSMITTED");
  captureStatus2->setText("Capture 2: CAPTURED");
  unlockButton->setEnabled(true);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == fftSizeSlider) {
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

int MainWindow::fftStepToValue(int step) const {
  return kFftMin + step * kFftStep;
}

int MainWindow::clampFftStep(int step) const {
  return std::clamp(step, 0, kFftSteps);
}

void MainWindow::applyFftSize(int fftValue) {
  fftSizeLabel->setText(QString("FFT: %1").arg(fftValue));
  if (receiver)
    receiver->setFftSize(fftValue);
  if (waterfallActive)
    waterfall->reset();
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
