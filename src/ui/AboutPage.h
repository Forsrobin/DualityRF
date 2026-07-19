#pragma once

#include <QWidget>

namespace duality {

// Static ABOUT view (third page of the main stack, next to CAPTURE and
// DEBUG): what the application is, version info and how to contribute.
class AboutPage : public QWidget {
    Q_OBJECT
public:
    explicit AboutPage(QWidget *parent = nullptr);
};

} // namespace duality
