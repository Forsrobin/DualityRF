#pragma once

#include "storage/SessionStore.h"

#include <QVector>
#include <QWidget>

#include <memory>

class QComboBox;
class QPushButton;
class QTableWidget;

namespace duality {

class SpectrumWidget;
class WaterfallWidget;

// Offline analysis view: open a session, compare two recordings (A/B) with
// overlaid average FFTs, per-recording waterfall, side-by-side metadata and
// measured signal characteristics; replay either recording.
class DebugWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit DebugWorkspace(SessionStore *store, QWidget *parent = nullptr);

    void refreshSessions();

signals:
    void replayRequested(const QString &absPath,
                         const duality::RecordingMetadata &meta);

private slots:
    void onSessionChanged(int index);
    void onRecordingChanged();

private:
    struct Analysis {
        QVector<float> spectrumDb;
        RecordingMetadata meta;
        bool valid = false;
        double occupiedBandwidthHz = 0.0;
        double meanDb = -120.0;
        double peakDb = -120.0;
        double peakOffsetHz = 0.0;
    };

    Analysis analyzeRecording(int recordingIndex);
    void updateViews();
    void fillTable(const Analysis &a, const Analysis &b);

    SessionStore *m_store;
    std::unique_ptr<Session> m_session;

    QComboBox *m_sessionCombo;
    QComboBox *m_recordingA;
    QComboBox *m_recordingB;
    QPushButton *m_replayA;
    QPushButton *m_replayB;
    SpectrumWidget *m_spectrum;
    WaterfallWidget *m_waterfall;
    QTableWidget *m_table;
};

} // namespace duality
