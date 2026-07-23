#include "ui/panels/LookingPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>

namespace duality {

namespace {

// Filled red STOP look, matched to the other programs' running state.
const auto kStopStyle = QStringLiteral(
    "QPushButton { background:#cc0000; color:#ffffff;"
    " border:1px solid #990000; font-weight:bold; }"
    "QPushButton:pressed { background:#990000; }");

} // namespace

LookingPanel::LookingPanel(QWidget *parent)
    : QWidget(parent)
    , m_frequency(new QDoubleSpinBox(this))
    , m_sampleRate(new QDoubleSpinBox(this))
    , m_bandwidth(new QDoubleSpinBox(this))
    , m_gain(new QDoubleSpinBox(this))
    , m_peakDetection(new QCheckBox(tr("Peak detection"), this))
    , m_peakThreshold(new QDoubleSpinBox(this))
    , m_peakBox(new QWidget(this))
    , m_startButton(new QPushButton(tr("START"), this))
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

    m_startButton->setCursor(Qt::PointingHandCursor);

    // The peak level lives in its own row container so it can be shown/hidden as
    // a unit with the detection toggle.
    auto *peakForm = new QFormLayout(m_peakBox);
    peakForm->setContentsMargins(0, 0, 0, 0);
    peakForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    peakForm->addRow(tr("Peak level"), m_peakThreshold);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addRow(m_startButton);
    layout->addRow(tr("Frequency"), m_frequency);
    layout->addRow(tr("Sample rate"), m_sampleRate);
    layout->addRow(tr("Bandwidth"), m_bandwidth);
    layout->addRow(tr("RX gain"), m_gain);
    layout->addRow(m_peakDetection);
    layout->addRow(m_peakBox);

    // One button that starts monitoring and, while running, turns into STOP.
    connect(m_startButton, &QPushButton::clicked, this, [this] {
        if (m_running)
            emit stopRequested();
        else
            emit startRequested();
    });
    for (QDoubleSpinBox *box : {m_frequency, m_sampleRate, m_bandwidth})
        connect(box, &QDoubleSpinBox::valueChanged, this,
                &LookingPanel::paramsChanged);
    connect(m_peakThreshold, &QDoubleSpinBox::valueChanged, this,
            &LookingPanel::peakThresholdChanged);
    // Enabling detection reveals the editable peak level.
    connect(m_peakDetection, &QCheckBox::toggled, this, [this](bool on) {
        m_peakBox->setVisible(on);
        emit peakDetectionToggled(on);
    });
    m_peakBox->setVisible(m_peakDetection->isChecked());

    setRunning(false);
}

StreamParams LookingPanel::streamParams() const
{
    StreamParams p;
    p.frequencyHz = m_frequency->value() * 1e6;
    p.sampleRateHz = m_sampleRate->value() * 1e6;
    p.bandwidthHz = m_bandwidth->value() * 1e6;
    p.gainDb = m_gain->value();
    return p;
}

bool LookingPanel::peakDetectionEnabled() const
{
    return m_peakDetection->isChecked();
}

double LookingPanel::peakThresholdDb() const
{
    return m_peakThreshold->value();
}

void LookingPanel::setRunning(bool running)
{
    m_running = running;
    m_startButton->setText(running ? tr("STOP") : tr("START"));
    m_startButton->setStyleSheet(running ? kStopStyle : QString());
}

} // namespace duality
