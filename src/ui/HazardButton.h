#pragma once

#include <QPushButton>

namespace duality {

// A checkable button that paints black/yellow hazard stripes when checked, with
// its label in a white box (black text) — used to make an active transmission
// unmistakable. When unchecked it falls back to the normal themed button look.
class HazardButton : public QPushButton {
    Q_OBJECT
public:
    explicit HazardButton(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

} // namespace duality
