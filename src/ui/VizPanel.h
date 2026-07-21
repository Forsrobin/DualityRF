#pragma once

#include <QWidget>

class QStackedWidget;

namespace duality {

// Bottom-strip visualization used by the Fob and Debug pages: shows either the
// waterfall or the FFT trace, one at a time, toggled by a small two-way switch.
// The two views are owned by the caller and simply re-parented into the panel.
class VizPanel : public QWidget {
    Q_OBJECT
public:
    enum Mode { Waterfall = 0, Fft = 1 };

    // waterfall and spectrum are the caller's view widgets (in that stack
    // order). The panel takes them as children and drives their visibility.
    VizPanel(QWidget *waterfall, QWidget *spectrum, QWidget *parent = nullptr);

    void setMode(Mode mode);

private:
    QStackedWidget *m_stack;
};

} // namespace duality
