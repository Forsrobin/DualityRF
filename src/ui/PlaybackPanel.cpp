#include "ui/PlaybackPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

namespace duality {

PlaybackPanel::PlaybackPanel(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_sessionCombo(new QComboBox(this))
    , m_recordingCombo(new QComboBox(this))
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
    m_gain->setValue(34.0);
    m_gain->setSuffix(tr(" dB"));

    m_speed->addItems({QStringLiteral("0.25x"), QStringLiteral("0.5x"),
                       QStringLiteral("1x"), QStringLiteral("2x"),
                       QStringLiteral("4x")});
    m_speed->setCurrentIndex(2);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addRow(tr("Session"), m_sessionCombo);
    layout->addRow(tr("Recording"), m_recordingCombo);
    layout->addRow(tr("Frequency"), m_frequency);
    layout->addRow(tr("Sample rate"), m_sampleRate);
    layout->addRow(tr("TX gain"), m_gain);
    layout->addRow(tr("Speed"), m_speed);
    layout->addRow(m_repeat);
    layout->addRow(m_playButton);
    layout->addRow(m_stopButton);
    layout->addRow(m_progressLabel);

    connect(m_sessionCombo, &QComboBox::currentIndexChanged, this,
            &PlaybackPanel::onSessionChanged);
    connect(m_recordingCombo, &QComboBox::currentIndexChanged, this,
            &PlaybackPanel::applyCurrent);
    connect(m_playButton, &QPushButton::clicked, this,
            &PlaybackPanel::playRequested);
    connect(m_stopButton, &QPushButton::clicked, this,
            &PlaybackPanel::stopRequested);
    setPlaying(false);
    refresh();
}

void PlaybackPanel::refresh()
{
    // Preserve the current session across a reload if it still exists.
    const QString previous = m_sessionCombo->currentData().toString();
    m_sessionCombo->blockSignals(true);
    m_sessionCombo->clear();
    for (const QString &dir : m_store->listSessionDirs())
        m_sessionCombo->addItem(QDir(dir).dirName(), dir);
    int index = m_sessionCombo->findData(previous);
    if (index < 0)
        index = m_sessionCombo->count() > 0 ? 0 : -1; // newest, or none
    m_sessionCombo->setCurrentIndex(index);
    m_sessionCombo->blockSignals(false);
    onSessionChanged(index);
}

void PlaybackPanel::select(const QString &sessionDir, const QString &file)
{
    refresh();
    const int sessionIdx = m_sessionCombo->findData(sessionDir);
    if (sessionIdx < 0)
        return;
    m_sessionCombo->blockSignals(true);
    m_sessionCombo->setCurrentIndex(sessionIdx);
    m_sessionCombo->blockSignals(false);
    onSessionChanged(sessionIdx); // load that session's recordings

    const int recIdx = m_recordingCombo->findText(file);
    if (recIdx >= 0)
        m_recordingCombo->setCurrentIndex(recIdx); // applyCurrent via signal
}

void PlaybackPanel::onSessionChanged(int index)
{
    m_session.reset();
    m_recordingCombo->blockSignals(true);
    m_recordingCombo->clear();
    if (index >= 0) {
        m_session = Session::load(m_sessionCombo->itemData(index).toString());
        if (m_session)
            for (const RecordingMetadata &meta : m_session->recordings())
                m_recordingCombo->addItem(meta.file);
    }
    const int recIdx = m_recordingCombo->count() > 0 ? 0 : -1;
    m_recordingCombo->setCurrentIndex(recIdx);
    m_recordingCombo->blockSignals(false);
    applyCurrent(recIdx);
}

void PlaybackPanel::applyCurrent(int index)
{
    if (!m_session || index < 0 || index >= m_session->recordings().size()) {
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
    if (!playing)
        m_progressLabel->clear();
}

void PlaybackPanel::setProgress(double seconds, double totalSeconds)
{
    m_progressLabel->setText(
        tr("%1 s / %2 s").arg(seconds, 0, 'f', 1).arg(totalSeconds, 0, 'f', 1));
}

} // namespace duality
