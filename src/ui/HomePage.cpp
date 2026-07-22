#include "ui/HomePage.h"

#include "core/Version.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QSize>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace duality {

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
    , m_grid(new QGridLayout)
    , m_clock(new QLabel(this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header band: logo + bold wordmark across the top, above the tiles.
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("homeHeader"));
    header->setFixedHeight(kHeaderHeight);
    header->setStyleSheet(
        QStringLiteral("#homeHeader { border-bottom: 1px solid #ffffff; }"));
    auto *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(10);
    headerRow->addStretch(1);

    const QString headerLabelStyle =
        QStringLiteral("background: transparent; border: none;");
    auto *logo = new QLabel(header);
    logo->setPixmap(QPixmap(QStringLiteral(":/assets/logo.png"))
                        .scaledToHeight(44, Qt::SmoothTransformation));
    logo->setStyleSheet(headerLabelStyle);
    headerRow->addWidget(logo);

    auto *title = new QLabel(QStringLiteral("DualityRF"), header);
    title->setStyleSheet(headerLabelStyle +
                         QStringLiteral(" font-weight: bold; font-size: 22px;"));
    headerRow->addWidget(title);
    headerRow->addStretch(1);
    root->addWidget(header);

    // Program grid: fixed-height tiles pinned to the top, three per row.
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(0);
    for (int col = 0; col < kColumns; ++col)
        m_grid->setColumnStretch(col, 1);
    root->addLayout(m_grid);
    root->addStretch(1); // keep tiles top-aligned, footer at the bottom

    // Footer: version on the left, live clock on the right.
    auto *footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("homeFooter"));
    footer->setFixedHeight(kFooterHeight);
    footer->setStyleSheet(
        QStringLiteral("#homeFooter { border-top: 1px solid #ffffff; }"));
    auto *footerRow = new QHBoxLayout(footer);
    footerRow->setContentsMargins(8, 0, 8, 0);

    // Transparent labels so they don't paint over the footer's top border.
    const QString footerLabelStyle =
        QStringLiteral("background: transparent; border: none;");
    auto *version = new QLabel(
        QStringLiteral("v%1").arg(QLatin1String(kVersion)), footer);
    version->setStyleSheet(footerLabelStyle);
    footerRow->addWidget(version);
    footerRow->addStretch(1);
    m_clock->setStyleSheet(footerLabelStyle);
    m_clock->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    footerRow->addWidget(m_clock);
    root->addWidget(footer);

    // Tick the clock once a second (HH:MM:SS).
    const auto tick = [this] {
        m_clock->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
    };
    tick();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, tick);
    timer->start(1000);
}

void HomePage::addProgram(const QString &name, const QString &iconPath)
{
    auto *tile = new QToolButton(this);
    tile->setText(name);
    tile->setIcon(QIcon(iconPath));
    tile->setIconSize(QSize(28, 28));
    tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tile->setCursor(Qt::PointingHandCursor);
    tile->setFixedHeight(kTileHeight);
    tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // White tile, gray border, black text (inverse of the default button).
    tile->setStyleSheet(QStringLiteral(
        "QToolButton { background: #ffffff; color: #000000;"
        " border: 1px solid #808080; }"
        "QToolButton:pressed { background: #d0d0d0; }"));

    const int index = m_count++;
    connect(tile, &QToolButton::clicked, this,
            [this, index] { emit programActivated(index); });
    m_grid->addWidget(tile, index / kColumns, index % kColumns);
}

} // namespace duality
