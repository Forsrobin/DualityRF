#include "ui/programs/CaptureProgram.h"

#include "ui/components/FlowLayout.h"
#include "ui/panels/CaptureControlPanel.h"
#include "ui/panels/SessionList.h"
#include "ui/panels/VizPanel.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/widgets/WaterfallWidget.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace duality {

CaptureProgram::CaptureProgram(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_capturePanel(new CaptureControlPanel(this))
    , m_sessionList(new SessionList(store, this))
    , m_spectrumView(new SpectrumWidget(this))
    , m_waterfallView(new WaterfallWidget(this))
{
    // Same wrapping tab-row pattern as the FOB program, but with only the two
    // panels this program needs. There is no visualization strip here.
    auto *panelStack = new QStackedWidget(this);
    auto *tabBar = new QWidget(this);
    tabBar->setObjectName(QStringLiteral("panelTabBar"));
    tabBar->setStyleSheet(
        QStringLiteral("#panelTabBar { border-bottom: 1px solid #ffffff; }"));
    auto *tabFlow = new FlowLayout(tabBar, 4, 4, 4);
    auto *tabGroup = new QButtonGroup(this);
    tabGroup->setExclusive(true);

    const auto addTab = [&](const QString &title, QWidget *panel) {
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
    addTab(tr("SESSIONS"), m_sessionList);
    connect(tabGroup, &QButtonGroup::idClicked, panelStack,
            &QStackedWidget::setCurrentIndex);
    tabGroup->button(0)->setChecked(true);

    // Bottom half: the same switchable waterfall/FFT view of the live capture as
    // the FOB program, sharing the 1/2 split below the tab row.
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
