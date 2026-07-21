#include "ui/VizPanel.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace duality {

VizPanel::VizPanel(QWidget *waterfall, QWidget *spectrum, QWidget *parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
{
    m_stack->addWidget(waterfall); // Mode::Waterfall
    m_stack->addWidget(spectrum);  // Mode::Fft

    // Little two-way switch: exclusive FFT / WATERFALL toggle buttons. FFT is
    // first and the default view.
    auto *fftButton = new QPushButton(tr("FFT"), this);
    auto *wfallButton = new QPushButton(tr("WATERFALL"), this);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (QPushButton *button : {fftButton, wfallButton}) {
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
    }
    group->addButton(fftButton, Mode::Fft);
    group->addButton(wfallButton, Mode::Waterfall);
    fftButton->setChecked(true);
    m_stack->setCurrentIndex(Mode::Fft);
    connect(group, &QButtonGroup::idClicked, m_stack,
            &QStackedWidget::setCurrentIndex);

    // Full-width border under the switch row, matching the panel tab row.
    auto *switchRow = new QWidget(this);
    switchRow->setObjectName(QStringLiteral("vizSwitchRow"));
    switchRow->setStyleSheet(QStringLiteral(
        "#vizSwitchRow { border-bottom: 1px solid #ffffff; }"));
    auto *switchLayout = new QHBoxLayout(switchRow);
    switchLayout->setContentsMargins(4, 4, 4, 4);
    switchLayout->setSpacing(4);
    switchLayout->addWidget(fftButton);
    switchLayout->addWidget(wfallButton);
    switchLayout->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(switchRow);
    layout->addWidget(m_stack, 1);
}

void VizPanel::setMode(Mode mode)
{
    m_stack->setCurrentIndex(mode);
}

} // namespace duality
