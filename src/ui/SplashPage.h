#pragma once

#include <QWidget>

namespace duality {

// The boot screen shown inside the main window (stack page) before the home
// grid: centred logo, tagline and version on the themed black background.
class SplashPage : public QWidget {
    Q_OBJECT
public:
    explicit SplashPage(QWidget *parent = nullptr);
};

} // namespace duality
