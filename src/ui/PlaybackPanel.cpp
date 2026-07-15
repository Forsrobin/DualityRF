#include "ui/PlaybackPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

namespace duality {

PlaybackPanel::PlaybackPanel(QWidget *parent)
    : QWidget(parent)
    , m_recordingLabel(new QLabel(tr("(no recording selected)"), this))
    , m_frequency(new QDoubleSpinBox(this))
    , m_sampleRate(new QDoubleSpinBox(this))
    , m_gain(new QDoubleSpinBox(this))
    , m_speed(new QComboBox(this))
    , m_repeat(new QCheckBox(tr("Repeat"), this))
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
    m_gain->setValue(20.0);
    m_gain->setSuffix(tr(" dB"));

    m_speed->addItems({QStringLiteral("0.25x"), QStringLiteral("0.5x"),
                       QStringLiteral("1x"), QStringLiteral("2x"),
                       QStringLiteral("4x")});
    m_speed->setCurrentIndex(2);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addRow(tr("Recording"), m_recordingLabel);
    layout->addRow(tr("Frequency"), m_frequency);
    layout->addRow(tr("Sample rate"), m_sampleRate);
    layout->addRow(tr("TX gain"), m_gain);
    layout->addRow(tr("Speed"), m_speed);
    layout->addRow(m_repeat);
    layout->addRow(m_playButton);
    layout->addRow(m_stopButton);
    layout->addRow(m_progressLabel);

    connect(m_playButton, &QPushButton::clicked, this,
            &PlaybackPanel::playRequested);
    connect(m_stopButton, &QPushButton::clicked, this,
            &PlaybackPanel::stopRequested);
    setPlaying(false);
    m_playButton->setEnabled(false);
}

void PlaybackPanel::setRecording(const RecordingMetadata &meta,
                                 const QString &absPath)
{
    m_meta = meta;
    m_path = absPath;
    m_recordingLabel->setText(meta.file);
    m_frequency->setValue(meta.frequencyHz / 1e6);
    m_sampleRate->setValue(meta.sampleRateHz / 1e6);
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
    if (!playing)
        m_progressLabel->clear();
}

void PlaybackPanel::setProgress(double seconds, double totalSeconds)
{
    m_progressLabel->setText(
        tr("%1 s / %2 s").arg(seconds, 0, 'f', 1).arg(totalSeconds, 0, 'f', 1));
}

} // namespace duality
