#include "ui/panels/PlaybackPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

namespace duality {

PlaybackPanel::PlaybackPanel(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_frequency(new QDoubleSpinBox(this))
    , m_sampleRate(new QDoubleSpinBox(this))
    , m_gain(new QDoubleSpinBox(this))
    , m_speed(new QComboBox(this))
    , m_repeat(new QCheckBox(tr("Repeat"), this))
    , m_progress(new QProgressBar(this))
    , m_playButton(new QPushButton(tr("TRANSMIT"), this))
    , m_stopButton(new QPushButton(tr("STOP"), this))
    , m_progressLabel(new QLabel(this))
{
    m_frequency->setRange(0.1, 7500.0);
    m_frequency->setDecimals(3);
    m_frequency->setSuffix(tr(" MHz"));

    m_sampleRate->setRange(0.1, 61.44);
    m_sampleRate->setDecimals(3);
    m_sampleRate->setSuffix(tr(" MS/s"));

    m_gain->setRange(0.0, 60.0);
    m_gain->setDecimals(1);
    m_gain->setValue(26.0);
    m_gain->setSuffix(tr(" dB"));

    m_speed->addItems({QStringLiteral("0.25x"), QStringLiteral("0.5x"),
                       QStringLiteral("1x"), QStringLiteral("2x"),
                       QStringLiteral("4x")});
    m_speed->setCurrentIndex(2);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addRow(tr("Frequency"), m_frequency);
    layout->addRow(tr("Sample rate"), m_sampleRate);
    layout->addRow(tr("TX gain"), m_gain);
    layout->addRow(tr("Speed"), m_speed);
    layout->addRow(m_repeat);
    // Slim progress bar above TRANSMIT tracks how far through the recording the
    // transmission is.
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    layout->addRow(m_progress);
    layout->addRow(m_playButton);
    layout->addRow(m_stopButton);
    layout->addRow(m_progressLabel);

    connect(m_playButton, &QPushButton::clicked, this,
            &PlaybackPanel::playRequested);
    connect(m_stopButton, &QPushButton::clicked, this,
            &PlaybackPanel::stopRequested);
    setPlaying(false);
}

void PlaybackPanel::refresh()
{
    // Selection comes from the session list, so there is nothing to repopulate;
    // just re-validate the current recording in case it was deleted elsewhere.
    if (!m_path.isEmpty() && m_session)
        select(m_session->dir(), m_meta.file);
}

void PlaybackPanel::select(const QString &sessionDir, const QString &file)
{
    m_session = Session::load(sessionDir);
    int index = -1;
    if (m_session) {
        const auto &recordings = m_session->recordings();
        for (int i = 0; i < recordings.size(); ++i)
            if (recordings[i].file == file) {
                index = i;
                break;
            }
    }
    if (index < 0) {
        m_meta = RecordingMetadata{};
        m_path.clear();
        m_playButton->setEnabled(false);
        return;
    }
    m_meta = m_session->recordings()[index];
    m_path = m_session->recordingPath(m_meta);
    m_frequency->setValue(m_meta.frequencyHz / 1e6);
    m_sampleRate->setValue(m_meta.sampleRateHz / 1e6);
    m_playButton->setEnabled(true);
}

StreamParams PlaybackPanel::streamParams() const
{
    StreamParams p;
    p.frequencyHz = m_frequency->value() * 1e6;
    p.sampleRateHz = m_sampleRate->value() * 1e6;
    p.gainDb = m_gain->value();
    return p;
}

double PlaybackPanel::speed() const
{
    static constexpr double kSpeeds[] = {0.25, 0.5, 1.0, 2.0, 4.0};
    return kSpeeds[m_speed->currentIndex()];
}

bool PlaybackPanel::repeat() const
{
    return m_repeat->isChecked();
}

void PlaybackPanel::setPlaying(bool playing)
{
    m_playButton->setEnabled(!playing && hasRecording());
    m_stopButton->setEnabled(playing);
    if (!playing) {
        m_progressLabel->clear();
        m_progress->setValue(0);
    }
}

void PlaybackPanel::setProgress(double seconds, double totalSeconds)
{
    m_progressLabel->setText(
        tr("%1 s / %2 s").arg(seconds, 0, 'f', 1).arg(totalSeconds, 0, 'f', 1));
    if (totalSeconds > 0.0) {
        const double fraction = qBound(0.0, seconds / totalSeconds, 1.0);
        m_progress->setValue(static_cast<int>(fraction * 1000.0));
    }
}

} // namespace duality
