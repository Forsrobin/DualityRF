#include "ui/AboutPage.h"

#include "core/Version.h"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace duality {

namespace {
constexpr const char *kRepositoryUrl =
    "https://github.com/Forsrobin/DualityRF";

QLabel *makeHeading(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral(
        "background:#ffffff; color:#000000; padding:6px 10px; "
        "font-weight:bold;"));
    return label;
}

QLabel *makeBody(const QString &html, QWidget *parent)
{
    auto *label = new QLabel(html, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    return label;
}

QString link(const QString &url, const QString &text)
{
    return QStringLiteral("<a href=\"%1\" style=\"color:#ffffff;\">%2</a>")
        .arg(url, text);
}
} // namespace

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    // Content column, centered and width-capped so lines stay readable on a
    // wide monitor; the whole page scrolls on small screens.
    auto *content = new QWidget;
    content->setMaximumWidth(760);
    auto *column = new QVBoxLayout(content);
    column->setSpacing(16);
    column->setContentsMargins(24, 24, 24, 24);

    auto *logo = new QLabel(content);
    logo->setPixmap(QPixmap(QStringLiteral(":/assets/logo.png"))
                        .scaled(220, 220, Qt::KeepAspectRatio,
                                Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignHCenter);
    column->addWidget(logo);

    auto *title = new QLabel(
        QStringLiteral("%1 v%2").arg(QLatin1String(kAppName),
                                     QLatin1String(kVersion)),
        content);
    title->setAlignment(Qt::AlignHCenter);
    title->setStyleSheet(
        QStringLiteral("font-size:22px; font-weight:bold;"));
    column->addWidget(title);

    column->addWidget(makeHeading(tr("WHAT IS DUALITY RF"), content));
    column->addWidget(makeBody(
        tr("DUALITY RF is a local desktop SDR laboratory for recording, "
           "visualizing, analyzing and replaying IQ sample streams from any "
           "SoapySDR-compatible device (RTL-SDR, HackRF, ADALM-PLUTO, "
           "LimeSDR, BladeRF, …).<br><br>"
           "Everything runs fully offline: captures are stored in local "
           "session directories together with their metadata, and can be "
           "compared side by side in the DEBUG workspace or transmitted "
           "again through any transmit-capable device."),
        content));

    column->addWidget(makeHeading(tr("HIGHLIGHTS"), content));
    column->addWidget(makeBody(
        tr("&bull; Live FFT spectrum and waterfall, processed off the UI "
           "thread<br>"
           "&bull; Manual, timed and auto-triggered capture with per-"
           "transmission segments<br>"
           "&bull; Optional concurrent TX (noise, CW, offset sine or custom "
           "IQ) while capturing<br>"
           "&bull; Structured sessions with metadata and an FFT cache<br>"
           "&bull; Playback of any recording through a selected "
           "transmitter<br>"
           "&bull; A/B debug workspace: overlaid FFTs, offline waterfall and "
           "signal measurements"),
        content));

    column->addWidget(makeHeading(tr("PROJECT INFO"), content));
    column->addWidget(makeBody(
        tr("Built with Qt %1, C++20, SoapySDR and FFTW.<br>"
           "Source code: %2")
            .arg(QLatin1String(qVersion()),
                 link(QLatin1String(kRepositoryUrl),
                      QLatin1String(kRepositoryUrl))),
        content));

    column->addWidget(makeHeading(tr("CONTRIBUTING"), content));
    column->addWidget(makeBody(
        tr("Contributions are welcome — bug reports, hardware compatibility "
           "notes, features and documentation alike.<br><br>"
           "&bull; Report bugs or request features at %1<br>"
           "&bull; To contribute code: fork the repository, create a feature "
           "branch, and open a pull request<br>"
           "&bull; Build instructions and the development environment are "
           "described in the %2<br>"
           "&bull; Design documents live in the repository under "
           "<i>docs/</i>")
            .arg(link(QLatin1String(kRepositoryUrl) + QLatin1String("/issues"),
                      tr("issue tracker")),
                 link(QLatin1String(kRepositoryUrl) +
                          QLatin1String("#getting-started"),
                      tr("README"))),
        content));

    column->addStretch(1);

    // Center the capped column inside the scroll area.
    auto *centered = new QWidget;
    auto *centerRow = new QHBoxLayout(centered);
    centerRow->setContentsMargins(0, 0, 0, 0);
    centerRow->addStretch(1);
    centerRow->addWidget(content);
    centerRow->addStretch(1);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(centered);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);
}

} // namespace duality
