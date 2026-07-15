#include "core/Version.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DualityRF"));
    QApplication::setApplicationName(duality::kAppName);
    QApplication::setApplicationVersion(duality::kVersion);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/logo.png")));

    duality::Theme::apply(app);

    duality::MainWindow window;
    window.show();
    return app.exec();
}
