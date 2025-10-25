
#include "MainWindow.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), running(false) {
  QWidget *central = new QWidget(this);
  setCentralWidget(central);
  setWindowTitle("Duality RF Console");

  setStyleSheet(R"(
        QWidget {
            background-color: black;
            color: cyan;
            font-family: monospace;
        }
        QDoubleSpinBox, QPushButton {
            background-color: #001010;
            color: cyan;
            border: 1px solid cyan;
            padding: 4px;
        }
        QLabel {
            color: cyan;
        }
    )");

  waterfall = new WaterfallWidget(this);

  rxFreq = new QDoubleSpinBox(this);
  txFreq = new QDoubleSpinBox(this);
  rxFreq->setSuffix(" MHz");
  txFreq->setSuffix(" MHz");
  rxFreq->setRange(0.1, 6000);
  txFreq->setRange(0.1, 6000);
  rxFreq->setValue(433.92);
  txFreq->setValue(433.95);

  startButton = new QPushButton("START", this);
  unlockButton = new QPushButton("UNLOCK", this);
  unlockButton->setEnabled(false);

  captureStatus1 = new QLabel("Capture 1: EMPTY");
  captureStatus2 = new QLabel("Capture 2: EMPTY");

  QHBoxLayout *freqLayout = new QHBoxLayout;
  freqLayout->addWidget(new QLabel("TX Frequency:"));
  freqLayout->addWidget(txFreq);
  freqLayout->addSpacing(20);
  freqLayout->addWidget(new QLabel("RX Frequency:"));
  freqLayout->addWidget(rxFreq);

  QFrame *line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  QVBoxLayout *layout = new QVBoxLayout(central);
  layout->addWidget(waterfall);
  layout->addWidget(line);
  layout->addLayout(freqLayout);
  layout->addWidget(startButton);
  layout->addWidget(captureStatus1);
  layout->addWidget(captureStatus2);
  layout->addWidget(unlockButton);
  central->setLayout(layout);

  receiver = new SDRReceiver(this);
  connect(receiver, &SDRReceiver::newFFTData, waterfall,
          &WaterfallWidget::pushData);

  connect(startButton, &QPushButton::clicked, this, &MainWindow::onStart);
  connect(unlockButton, &QPushButton::clicked, this,
          &MainWindow::onStateUpdate);
}

void MainWindow::onStart() {
  running = !running;
  startButton->setText(running ? "STOP" : "START");

  if (running) {
    captureStatus1->setText("Capture 1: CAPTURED");
    captureStatus2->setText("Capture 2: TRANSMITTED");
    unlockButton->setEnabled(true);

    double freq = rxFreq->value();
    receiver->start(freq);
  } else {
    receiver->stop();
    captureStatus1->setText("Capture 1: EMPTY");
    captureStatus2->setText("Capture 2: EMPTY");
    unlockButton->setEnabled(false);
  }
}

void MainWindow::onStateUpdate() {
  // Example post-transmit logic
  captureStatus1->setText("Capture 1: TRANSMITTED");
  captureStatus2->setText("Capture 2: CAPTURED");
  unlockButton->setEnabled(true);
}
