#include "ui/panels/CaptureControlPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

namespace duality {

namespace {

// Settings persisted across restarts. Kept separate from the FOB capture panel
// so the two programs remember their own tuning.
const auto kFrequencyKey = QStringLiteral("captureProg/frequencyMHz");
const auto kPeakThresholdKey = QStringLiteral("captureProg/peakThresholdDb");
const auto kCaptureRangeKey = QStringLiteral("captureProg/rangeKHz");
const auto kHangTimeKey = QStringLiteral("captureProg/hangTimeMs");

// Filled red STOP look, matched to the FOB capture panel.
const auto kStopStyle = QStringLiteral(
    "QPushButton { background:#cc0000; color:#ffffff;"
    " border:1px solid #990000; font-weight:bold; }"
    "QPushButton:pressed { background:#990000; }");

} // namespace

CaptureControlPanel::CaptureControlPanel(QWidget *parent)
    : QWidget(parent)
    , m_frequency(new QDoubleSpinBox(this))
    , m_sampleRate(new QDoubleSpinBox(this))
    , m_bandwidth(new QDoubleSpinBox(this))
    , m_gain(new QDoubleSpinBox(this))
    , m_peakThreshold(new QDoubleSpinBox(this))
    , m_captureRange(new QDoubleSpinBox(this))
    , m_hangTime(new QDoubleSpinBox(this))
    , m_mode(new QComboBox(this))
    , m_autoBox(new QWidget(this))
    , m_recordButton(new QPushButton(tr("RECORD"), this))
{
    m_frequency->setRange(0.1, 7500.0);
    m_frequency->setDecimals(3);
    m_frequency->setValue(433.8);
    m_frequency->setSuffix(tr(" MHz"));

    m_sampleRate->setRange(0.1, 61.44);
    m_sampleRate->setDecimals(3);
    m_sampleRate->setValue(2.0);
    m_sampleRate->setSuffix(tr(" MS/s"));

    m_bandwidth->setRange(0.0, 61.44);
    m_bandwidth->setDecimals(3);
    m_bandwidth->setValue(0.0);
    m_bandwidth->setSuffix(tr(" MHz"));
    m_bandwidth->setSpecialValueText(tr("auto"));

    m_gain->setRange(0.0, 76.0);
    m_gain->setDecimals(1);
    m_gain->setValue(32.0);
    m_gain->setSuffix(tr(" dB"));

    m_peakThreshold->setRange(-110.0, 0.0);
    m_peakThreshold->setDecimals(1);
    m_peakThreshold->setSingleStep(5.0);
    m_peakThreshold->setValue(-25.0);
    m_peakThreshold->setSuffix(tr(" dB"));

    m_captureRange->setRange(1.0, 5000.0);
    m_captureRange->setDecimals(1);
    m_captureRange->setSingleStep(10.0);
    m_captureRange->setValue(50.0);
    m_captureRange->setPrefix(tr("± "));
    m_captureRange->setSuffix(tr(" kHz"));

    m_hangTime->setRange(50.0, 5000.0);
    m_hangTime->setDecimals(0);
    m_hangTime->setSingleStep(50.0);
    m_hangTime->setValue(500.0);
    m_hangTime->setSuffix(tr(" ms"));

    m_mode->addItems({tr("manual"), tr("auto")});

    m_recordButton->setCursor(Qt::PointingHandCursor);

    // Auto-only detection settings live in their own container so the whole
    // group can be shown/hidden as one when the mode changes.
    auto *autoForm = new QFormLayout(m_autoBox);
    autoForm->setContentsMargins(0, 0, 0, 0);
    autoForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    // Section divider heading the auto-only detection controls.
    auto *autoHeader = new QLabel(tr("Peak detection settings"), m_autoBox);
    autoHeader->setStyleSheet(
        QStringLiteral("color:#aaaaaa; font-weight:bold; margin-top:6px; "
                       "padding-bottom:2px; border-bottom:1px solid #555555;"));
    autoForm->addRow(autoHeader);
    autoForm->addRow(tr("Peak level"), m_peakThreshold);
    autoForm->addRow(tr("Capture range"), m_captureRange);
    autoForm->addRow(tr("Hang time"), m_hangTime);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    // Mode and the single arming button up top so they stay in view.
    layout->addRow(tr("Mode"), m_mode);
    layout->addRow(m_recordButton);
    layout->addRow(tr("Frequency"), m_frequency);
    layout->addRow(tr("Sample rate"), m_sampleRate);
    layout->addRow(tr("Bandwidth"), m_bandwidth);
    layout->addRow(tr("RX gain"), m_gain);
    layout->addRow(m_autoBox);

    // One button that arms recording and, while running, turns into a red STOP.
    connect(m_recordButton, &QPushButton::clicked, this, [this] {
        if (m_running)
            emit stopRequested();
        else
            emit recordRequested();
    });
    for (QDoubleSpinBox *box : {m_frequency, m_sampleRate, m_bandwidth,
                                m_captureRange})
        connect(box, &QDoubleSpinBox::valueChanged, this,
                &CaptureControlPanel::paramsChanged);
    connect(m_peakThreshold, &QDoubleSpinBox::valueChanged, this,
            &CaptureControlPanel::peakThresholdChanged);

    // Only auto mode exposes the detection settings.
    const auto applyMode = [this] {
        m_autoBox->setVisible(m_mode->currentText() == QLatin1String("auto"));
    };
    connect(m_mode, &QComboBox::currentTextChanged, this,
            [this, applyMode] {
                applyMode();
                emit paramsChanged();
            });
    applyMode();

    // Restore the last used values, then persist every edit.
    QSettings settings;
    m_frequency->setValue(
        settings.value(kFrequencyKey, m_frequency->value()).toDouble());
    m_peakThreshold->setValue(
        settings.value(kPeakThresholdKey, m_peakThreshold->value())
            .toDouble());
    m_captureRange->setValue(
        settings.value(kCaptureRangeKey, m_captureRange->value()).toDouble());
    m_hangTime->setValue(
        settings.value(kHangTimeKey, m_hangTime->value()).toDouble());
    const auto persist = [this](QDoubleSpinBox *box, const QString &key) {
        connect(box, &QDoubleSpinBox::valueChanged, this,
                [key](double value) { QSettings().setValue(key, value); });
    };
    persist(m_frequency, kFrequencyKey);
    persist(m_peakThreshold, kPeakThresholdKey);
    persist(m_captureRange, kCaptureRangeKey);
    persist(m_hangTime, kHangTimeKey);

    setRunning(false);
}

StreamParams CaptureControlPanel::streamParams() const
{
    StreamParams p;
    p.frequencyHz = m_frequency->value() * 1e6;
    p.sampleRateHz = m_sampleRate->value() * 1e6;
    p.bandwidthHz = m_bandwidth->value() * 1e6;
    p.gainDb = m_gain->value();
    return p;
}

QString CaptureControlPanel::trigger() const
{
    return m_mode->currentText();
}

double CaptureControlPanel::peakThresholdDb() const
{
    return m_peakThreshold->value();
}

double CaptureControlPanel::captureRangeHz() const
{
    // Manual mode captures the full band: a zero range makes the pipeline skip
    // trimming and hides the capture-range overlay on the FFT. Only auto mode
    // limits (and marks) the band.
    if (m_mode->currentText() != QLatin1String("auto"))
        return 0.0;
    return m_captureRange->value() * 1e3;
}

double CaptureControlPanel::hangTimeMs() const
{
    return m_hangTime->value();
}

void CaptureControlPanel::setRunning(bool running)
{
    m_running = running;
    m_recordButton->setText(running ? tr("STOP") : tr("RECORD"));
    m_recordButton->setStyleSheet(running ? kStopStyle : QString());
    // Locking the mode while running keeps it consistent with the live capture.
    m_mode->setEnabled(!running);
}

} // namespace duality
