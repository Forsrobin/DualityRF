
#include "ui/MainWindow.h"
#include "ui/SplashScreen.h"
#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  SplashScreen splash;
  MainWindow mainWin;

  QObject::connect(&splash, &SplashScreen::bothDevicesReady, [&]() {
    splash.hide();
    mainWin.show();
  });

  splash.show();
  return app.exec();
}
