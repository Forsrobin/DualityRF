
#pragma once
#include "../core/SDRManager.h"
#include <QLabel>
#include <QTimer>
#include <QWidget>

class SplashScreen : public QWidget {
  Q_OBJECT
public:
  explicit SplashScreen(QWidget *parent = nullptr);

signals:
  void bothDevicesReady();

private slots:
  void checkDevices();

private:
  QLabel *logo;
  QLabel *status;
  SDRManager *manager;
  QTimer *pollTimer;
};
