#include "ui/panels/CapturePanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

namespace duality {

namespace {

// Settings persisted across restarts, like the device selection.
const auto kFrequencyKey = QStringLiteral("capture/frequencyMHz");
const auto kPeakThresholdKey = QStringLiteral("capture/peakThresholdDb");
const auto kCaptureRangeKey = QStringLiteral("capture/rangeKHz");
const auto kHangTimeKey = QStringLiteral("capture/hangTimeMs");

} // namespace

CapturePanel::CapturePanel(QWidget *parent)
    : QWidget(parent)
    , m_frequency(new QDoubleSpinBox(this))
    , m_sampleRate(new QDoubleSpinBox(this))
    , m_bandwidth(new QDoubleSpinBox(this))
    , m_gain(new QDoubleSpinBox(this))
    , m_peakThreshold(new QDoubleSpinBox(this))
    , m_captureRange(new QDoubleSpinBox(this))
    , m_hangTime(new QDoubleSpinBox(this))
    , m_trigger(new QComboBox(this))
    , m_replayMode(new QPushButton(tr("REPLAY MODE"), this))
    , m_txIndicator(new QLabel(tr("TX"), this))
    , m_monitorButton(new QPushButton(tr("MONITOR"), this))
    , m_recordButton(new QPushButton(tr("RECORD"), this))
    , m_stopButton(new QPushButton(tr("STOP"), this))
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

    m_trigger->addItems({tr("manual"), tr("auto")});

    m_replayMode->setCheckable(true);
    // Yellow flags that replay mode will transmit; the active state is a
    // filled, bold, white-bordered button vs. the muted outline when off.
    m_replayMode->setStyleSheet(QStringLiteral(
        "QPushButton { background:#4d4000; color:#f5c400; "
        "border:1px solid #f5c400; }"
        "QPushButton:checked { background:#f5c400; color:#000000; "
        "border:2px solid #ffffff; font-weight:bold; }"));

    // Small "TX" lamp shown next to REPLAY MODE; lit when concurrent TX is on.
    m_txIndicator->setAlignment(Qt::AlignCenter);
    setConcurrentTxEnabled(false);

    // Replay mode sits beside its concurrent-TX indicator.
    auto *replayRow = new QHBoxLayout;
    replayRow->addWidget(m_replayMode, 1);
    replayRow->addWidget(m_txIndicator);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    // Trigger and the arming buttons up top so they stay in view without
    // scrolling the form.
    layout->addRow(tr("Trigger"), m_trigger);
    layout->addRow(m_monitorButton);
    layout->addRow(m_recordButton);
    layout->addRow(replayRow);
    layout->addRow(m_stopButton);
    layout->addRow(tr("Frequency"), m_frequency);
    layout->addRow(tr("Sample rate"), m_sampleRate);
    layout->addRow(tr("Bandwidth"), m_bandwidth);
    layout->addRow(tr("RX gain"), m_gain);
    layout->addRow(tr("Peak level"), m_peakThreshold);
    layout->addRow(tr("Capture range"), m_captureRange);
    layout->addRow(tr("Hang time"), m_hangTime);

    connect(m_monitorButton, &QPushButton::clicked, this,
            &CapturePanel::monitorRequested);
    connect(m_recordButton, &QPushButton::clicked, this,
            &CapturePanel::recordRequested);
    connect(m_stopButton, &QPushButton::clicked, this,
            &CapturePanel::stopRequested);
    for (QDoubleSpinBox *box : {m_frequency, m_sampleRate, m_bandwidth,
                                m_captureRange})
        connect(box, &QDoubleSpinBox::valueChanged, this,
                &CapturePanel::paramsChanged);
    connect(m_peakThreshold, &QDoubleSpinBox::valueChanged, this,
            &CapturePanel::peakThresholdChanged);
    // Replay mode only makes sense with the auto trigger, so force it to auto
    // and lock the selector while enabled.
    connect(m_replayMode, &QPushButton::toggled, this, [this](bool on) {
        if (on)
            m_trigger->setCurrentText(QStringLiteral("auto"));
        m_trigger->setEnabled(!on);
    });

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

StreamParams CapturePanel::streamParams() const
{
    StreamParams p;
    p.frequencyHz = m_frequency->value() * 1e6;
    p.sampleRateHz = m_sampleRate->value() * 1e6;
    p.bandwidthHz = m_bandwidth->value() * 1e6;
    p.gainDb = m_gain->value();
    return p;
}

QString CapturePanel::trigger() const
{
    return m_trigger->currentText();
}

double CapturePanel::peakThresholdDb() const
{
    return m_peakThreshold->value();
}

double CapturePanel::captureRangeHz() const
{
    return m_captureRange->value() * 1e3;
}

double CapturePanel::hangTimeMs() const
{
    return m_hangTime->value();
}

bool CapturePanel::replayMode() const
{
    return m_replayMode->isChecked();
}

void CapturePanel::setRunning(bool running)
{
    m_monitorButton->setEnabled(!running);
    m_recordButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
}

void CapturePanel::setConcurrentTxEnabled(bool enabled)
{
    m_txIndicator->setStyleSheet(
        enabled ? QStringLiteral("background:#f5c400; color:#000000; "
                                 "border:1px solid #ffffff; padding:2px 6px;")
                : QStringLiteral("background:#000000; color:#555555; "
                                 "border:1px solid #555555; padding:2px 6px;"));
    m_txIndicator->setToolTip(enabled ? tr("Concurrent TX enabled")
                                      : tr("Concurrent TX disabled"));
}

} // namespace duality
