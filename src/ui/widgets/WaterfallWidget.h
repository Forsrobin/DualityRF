#pragma once

#include <QImage>
#include <QVector>
#include <QWidget>

namespace duality {

// Scrolling spectral history. Rows are stored in a circular QImage so a new
// row costs one line write, not a full-image scroll (Raspberry Pi friendly).
class WaterfallWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaterfallWidget(QWidget *parent = nullptr);

    void addRow(const QVector<float> &db);
    void setRows(const QVector<QVector<float>> &rows); // offline rendering
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void ensureImage(int bins);
    void writeRow(int y, const QVector<float> &db);

    QImage m_image;
    int m_nextRow = 0;
    int m_filledRows = 0;

    static constexpr int kHistory = 512;
};

} // namespace duality
