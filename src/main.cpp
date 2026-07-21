#include "core/Version.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DualityRF"));
    // Also becomes the Wayland app_id, so a compositor rule can target the
    // popout, e.g. Hyprland: windowrulev2 = float, class:^(DualityRF)$
    QApplication::setApplicationName(duality::kAppName);
    QApplication::setApplicationVersion(duality::kVersion);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/logo.png")));

    duality::Theme::apply(app);

    // The boot splash is now the first page of the window itself, so just show
    // the window; it reveals the home grid after a short delay.
    duality::MainWindow window;
    window.show();
    return app.exec();
}
