#include "ui/SplashPage.h"

#include "core/Version.h"

#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

namespace duality {

SplashPage::SplashPage(QWidget *parent)
    : QWidget(parent)
{
    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(24, 24, 24, 24);
    column->setSpacing(12);
    column->addStretch(1);

    auto *logo = new QLabel(this);
    logo->setPixmap(QPixmap(QStringLiteral(":/assets/logo.png"))
                        .scaled(200, 200, Qt::KeepAspectRatio,
                                Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    column->addWidget(logo);

    auto *tagline = new QLabel(
        QStringLiteral("SDR CAPTURE · ANALYSIS · REPLAY"), this);
    tagline->setAlignment(Qt::AlignCenter);
    QFont taglineFont = tagline->font();
    taglineFont.setBold(true);
    taglineFont.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    tagline->setFont(taglineFont);
    column->addWidget(tagline);

    auto *version = new QLabel(
        QStringLiteral("v%1").arg(QLatin1String(kVersion)), this);
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet(QStringLiteral("color: #9a9a9a;"));
    column->addWidget(version);

    column->addStretch(1);

    auto *status = new QLabel(tr("Starting up…"), this);
    status->setAlignment(Qt::AlignCenter);
    status->setStyleSheet(QStringLiteral("color: #9a9a9a;"));
    column->addWidget(status);
}

} // namespace duality
