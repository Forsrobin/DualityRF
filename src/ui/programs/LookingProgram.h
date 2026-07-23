#pragma once

#include <QWidget>

namespace duality {

class LookingPanel;
class SpectrumWidget;
class WaterfallWidget;

// The "looking glass": a monitor-only analysis tool. Basic RX tuning and a
// peak-detection toggle over the same switchable waterfall/FFT view as FOB, with
// no tabs, recording or transmit. MainWindow drives the shared capture pipeline
// in monitor mode through the panel and renders into these views.
class LookingProgram : public QWidget {
    Q_OBJECT
public:
    explicit LookingProgram(QWidget *parent = nullptr);

    LookingPanel *panel() const { return m_panel; }
    SpectrumWidget *spectrumView() const { return m_spectrumView; }
    WaterfallWidget *waterfallView() const { return m_waterfallView; }

private:
    LookingPanel *m_panel;
    SpectrumWidget *m_spectrumView;
    WaterfallWidget *m_waterfallView;
};

} // namespace duality
