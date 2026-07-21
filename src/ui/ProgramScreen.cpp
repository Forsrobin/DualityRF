#include "ui/ProgramScreen.h"

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
    back->setText(QStringLiteral("‹"));
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(tr("Back to home"));
    // Square button with no padding so the glyph centres both ways.
    back->setFixedSize(38, 24);
    back->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 0px; font-size: 13px;"
        " min-width: 0px; min-height: 0px; }"));
    connect(back, &QToolButton::clicked, this, &ProgramScreen::backRequested);
    barRow->addWidget(back);

    auto *label = new QLabel(title, bar);
    label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    barRow->addWidget(label);
    barRow->addStretch(1);
    root->addWidget(bar);

    content->setParent(this);
    root->addWidget(content, 1);
}

} // namespace duality
