#include "ui/programs/FobProgram.h"

#include "ui/CapturePanel.h"
#include "ui/DevicePanel.h"
#include "ui/FlowLayout.h"
#include "ui/PlaybackPanel.h"
#include "ui/SessionBrowser.h"
#include "ui/SpectrumWidget.h"
#include "ui/TransmitPanel.h"
#include "ui/VizPanel.h"
#include "ui/WaterfallWidget.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace duality {

FobProgram::FobProgram(DeviceManager *deviceManager, SessionStore *store,
                       QWidget *parent)
    : QWidget(parent)
    , m_devicePanel(new DevicePanel(deviceManager, this))
    , m_capturePanel(new CapturePanel(this))
    , m_transmitPanel(new TransmitPanel(this))
    , m_playbackPanel(new PlaybackPanel(store, this))
    , m_sessionBrowser(new SessionBrowser(store, this))
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
    addTab(tr("DEVICES"), m_devicePanel);
    addTab(tr("CAPTURE"), m_capturePanel);
    addTab(tr("CONCURRENT TX"), m_transmitPanel);
    addTab(tr("SESSIONS"), m_sessionBrowser);
    addTab(tr("PLAYBACK"), m_playbackPanel);
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
