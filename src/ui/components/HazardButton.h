#pragma once

#include <QPushButton>

namespace duality {

// A checkable button that paints black/yellow hazard stripes when checked, with
// its label in a white box (black text) — used to make an active transmission
// unmistakable. When unchecked it falls back to the normal themed button look,
// unless setAlwaysHazard(true) keeps the stripes on in both states (e.g. a
// START button that should always read as a transmit trigger).
class HazardButton : public QPushButton {
    Q_OBJECT
public:
    explicit HazardButton(QWidget *parent = nullptr);

    // Paint the hazard stripes regardless of the checked state.
    void setAlwaysHazard(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_alwaysHazard = false;
};

} // namespace duality
