#include "ui/programs/FobProgram.h"

#include "ui/panels/CapturePanel.h"
#include "ui/components/FlowLayout.h"
#include "ui/panels/SessionList.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/panels/TransmitPanel.h"
#include "ui/panels/VizPanel.h"
#include "ui/widgets/WaterfallWidget.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace duality {

FobProgram::FobProgram(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_capturePanel(new CapturePanel(this))
    , m_transmitPanel(new TransmitPanel(this))
    , m_sessionList(new SessionList(store, this))
    , m_spectrumView(new SpectrumWidget(this))
    , m_waterfallView(new WaterfallWidget(this))
{
    // One tab row for every control panel (top 3/4). The tabs are checkable
    // buttons in a FlowLayout so they wrap to more rows instead of overflowing
    // the 320px width; each drives the panel stack below.
    auto *panelStack = new QStackedWidget(this);
    auto *tabBar = new QWidget(this);
    tabBar->setObjectName(QStringLiteral("panelTabBar"));
    // Full-width border under the wrapping tab row.
    tabBar->setStyleSheet(
        QStringLiteral("#panelTabBar { border-bottom: 1px solid #ffffff; }"));
    auto *tabFlow = new FlowLayout(tabBar, 4, 4, 4);
    auto *tabGroup = new QButtonGroup(this);
    tabGroup->setExclusive(true);

    const auto addTab = [&](const QString &title, QWidget *panel) {
        // Wrap each panel so a tall control set scrolls rather than being
        // squeezed into the short window.
        auto *scroll = new QScrollArea;
        scroll->setWidget(panel);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        const int index = panelStack->addWidget(scroll);

        auto *button = new QPushButton(title);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        tabGroup->addButton(button, index);
        tabFlow->addWidget(button);
    };
    addTab(tr("CAPTURE"), m_capturePanel);
    addTab(tr("CONCURRENT TX"), m_transmitPanel);
    addTab(tr("SESSIONS"), m_sessionList);
    connect(tabGroup, &QButtonGroup::idClicked, panelStack,
            &QStackedWidget::setCurrentIndex);
    tabGroup->button(0)->setChecked(true);

    // Bottom 1/4: a single visualization of the live capture, switchable
    // between the waterfall and the FFT trace.
    m_spectrumView->setMinimumHeight(60);
    m_waterfallView->setMinimumHeight(60);
    auto *viz = new VizPanel(m_waterfallView, m_spectrumView);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tabBar);
    layout->addWidget(panelStack, 1);
    layout->addWidget(viz, 1);
}

} // namespace duality
