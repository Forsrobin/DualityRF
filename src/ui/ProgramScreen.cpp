#include "ui/ProgramScreen.h"

#include "ui/components/DeviceBadge.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace duality {

ProgramScreen::ProgramScreen(const QString &title, QWidget *content,
                             QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Slim top bar: back arrow + program title, with a full-width underline to
    // match the other section dividers.
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("programBar"));
    // Fixed height so every program's bar matches (otherwise it sizes to its
    // content and varies between screens).
    bar->setFixedHeight(30);
    bar->setStyleSheet(
        QStringLiteral("#programBar { border-bottom: 1px solid #ffffff; }"));
    auto *barRow = new QHBoxLayout(bar);
    barRow->setContentsMargins(4, 2, 6, 2);
    barRow->setSpacing(6);

    auto *back = new QToolButton(bar);
    back->setText(QStringLiteral("‹  Back"));
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(tr("Back to home"));
    back->setFixedHeight(24);
    back->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 0 12px; font-size: 13px;"
        " min-width: 0px; min-height: 0px; }"));
    connect(back, &QToolButton::clicked, this, &ProgramScreen::backRequested);
    barRow->addWidget(back);

    auto *label = new QLabel(title, bar);
    label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    barRow->addWidget(label);
    barRow->addStretch(1);

    // Selected device indicators on the right: red TX, blue RX. Tapping either
    // opens the device selector; every program carries them so the current
    // devices are always visible.
    m_txBadge = new DeviceBadge(QStringLiteral("TX"), QStringLiteral("#c0392b"),
                                bar);
    m_rxBadge = new DeviceBadge(QStringLiteral("RX"), QStringLiteral("#2c6fbf"),
                                bar);
    connect(m_txBadge, &DeviceBadge::clicked, this, &ProgramScreen::txClicked);
    connect(m_rxBadge, &DeviceBadge::clicked, this, &ProgramScreen::rxClicked);
    barRow->addWidget(m_txBadge);
    barRow->addWidget(m_rxBadge);
    root->addWidget(bar);

    content->setParent(this);
    root->addWidget(content, 1);
}

void ProgramScreen::setTxName(const QString &name)
{
    m_txBadge->setDeviceName(name);
}

void ProgramScreen::setRxName(const QString &name)
{
    m_rxBadge->setDeviceName(name);
}

} // namespace duality
