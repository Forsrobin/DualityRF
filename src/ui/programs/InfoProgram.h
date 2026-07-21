#pragma once

#include <QWidget>

namespace duality {

// Static ABOUT view (third page of the main stack, next to CAPTURE and
// DEBUG): what the application is, version info and how to contribute.
class InfoProgram : public QWidget {
    Q_OBJECT
public:
    explicit InfoProgram(QWidget *parent = nullptr);
};

} // namespace duality
