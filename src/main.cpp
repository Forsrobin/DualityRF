#include "core/Version.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QSplashScreen>
#include <QTimer>

namespace {

// Minimum time the boot screen stays up; window construction is fast, so
// without a floor the splash would only flash for a frame.
constexpr int kSplashMinimumMs = 1500;

QPixmap buildSplashPixmap()
{
    const QSize canvas(520, 600);
    QPixmap pixmap(canvas);
    pixmap.fill(Qt::black);

    QPainter p(&pixmap);
    const QPixmap logo =
        QPixmap(QStringLiteral(":/assets/logo.png"))
            .scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    p.drawPixmap((canvas.width() - logo.width()) / 2, 48, logo);

    QFont font = p.font();
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 452, canvas.width(), 24), Qt::AlignCenter,
               QStringLiteral("SDR CAPTURE · ANALYSIS · REPLAY"));

    font.setBold(false);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    p.setFont(font);
    p.setPen(QColor(0x9a, 0x9a, 0x9a));
    p.drawText(QRect(0, 480, canvas.width(), 24), Qt::AlignCenter,
               QStringLiteral("v%1").arg(QLatin1String(duality::kVersion)));

    // Same 1px white frame the themed widgets use.
    p.setPen(Qt::white);
    p.drawRect(0, 0, canvas.width() - 1, canvas.height() - 1);
    return pixmap;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("DualityRF"));
    QApplication::setApplicationName(duality::kAppName);
    QApplication::setApplicationVersion(duality::kVersion);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/logo.png")));

    duality::Theme::apply(app);

    QSplashScreen splash(buildSplashPixmap());
    splash.showMessage(QObject::tr("Starting up…"),
                       Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
    splash.show();
    app.processEvents();

    duality::MainWindow window;
    QTimer::singleShot(kSplashMinimumMs, &window, [&window, &splash] {
        window.show();
        splash.finish(&window);
    });
    return app.exec();
}
