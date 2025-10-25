
#include "SplashScreen.h"
#include <QMovie>
#include <QPixmap>
#include <QVBoxLayout>

SplashScreen::SplashScreen(QWidget *parent) : QWidget(parent) {
  setStyleSheet(
      "background-color: black; color: cyan; font-family: monospace;");

  logo = new QLabel(this);
  logo->setPixmap(
      QPixmap(":/../assets/logo.png")
          .scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  logo->setAlignment(Qt::AlignCenter);

  status = new QLabel("Scanning for SDR devices...", this);
  status->setAlignment(Qt::AlignCenter);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addStretch();
  layout->addWidget(logo);
  layout->addWidget(status);
  layout->addStretch();
  setLayout(layout);

  manager = new SDRManager(this);
  pollTimer = new QTimer(this);
  connect(pollTimer, &QTimer::timeout, this, &SplashScreen::checkDevices);
  pollTimer->start(2000);
}

void SplashScreen::checkDevices() {
  manager->pollDevices();
  if (manager->hasRTLSDR() && manager->hasHackRF()) {
    status->setText("Both RTL-SDR and HackRF detected. Starting...");
    pollTimer->stop();
    emit bothDevicesReady();
  } else {
    QString s = QString("RTL: %1 | HackRF: %2")
                    .arg(manager->hasRTLSDR() ? "OK" : "Missing")
                    .arg(manager->hasHackRF() ? "OK" : "Missing");
    status->setText(s);
  }
}
