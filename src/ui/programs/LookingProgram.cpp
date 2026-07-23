#include "ui/programs/LookingProgram.h"

#include "ui/panels/LookingPanel.h"
#include "ui/panels/VizPanel.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/widgets/WaterfallWidget.h"

#include <QScrollArea>
#include <QVBoxLayout>

namespace duality {

LookingProgram::LookingProgram(QWidget *parent)
    : QWidget(parent)
    , m_panel(new LookingPanel(this))
    , m_spectrumView(new SpectrumWidget(this))
    , m_waterfallView(new WaterfallWidget(this))
{
    // Controls occupy a fixed top half that scrolls internally, so toggling the
    // peak-level field never resizes the controls or shifts the view below.
    auto *scroll = new QScrollArea(this);
    scroll->setWidget(m_panel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_spectrumView->setMinimumHeight(60);
    m_waterfallView->setMinimumHeight(60);
    auto *viz = new VizPanel(m_waterfallView, m_spectrumView);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(scroll, 1);
    layout->addWidget(viz, 1);
}

} // namespace duality
