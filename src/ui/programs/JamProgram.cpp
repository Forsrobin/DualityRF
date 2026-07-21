#include "ui/programs/JamProgram.h"

#include "ui/components/HazardButton.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/widgets/WaterfallWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <array>

namespace duality {

namespace {
// Frequently used transmit spots: name, carrier (MHz), span (MS/s). The first
// entry is a free-form "Custom" that leaves the fields untouched.
struct Preset {
    const char *name;
    double frequencyMHz;
    double sampleRateMSps;
};
constexpr std::array<Preset, 6> kPresets{{
    {"Custom", 0.0, 0.0},
    {"315 MHz (ISM)", 315.0, 2.0},
    {"433.92 MHz (ISM)", 433.92, 2.0},
    {"868 MHz (ISM)", 868.0, 2.0},
    {"915 MHz (ISM)", 915.0, 2.0},
    {"2450 MHz (ISM)", 2450.0, 10.0},
}};

// Waveform choices, matching the FOB concurrent-TX panel.
const WaveformType kTypes[] = {
    WaveformType::WhiteNoise,   WaveformType::GaussianNoise,
    WaveformType::ContinuousWave, WaveformType::Sine,
    WaveformType::IqFile,
};

// Fixed amplitude: the TX gain is the only level control in this program.
constexpr double kAmplitude = 0.5;
} // namespace

JamProgram::JamProgram(QWidget *parent)
    : QWidget(parent)
    , m_preset(new QComboBox(this))
    , m_frequency(new QDoubleSpinBox(this))
    , m_sampleRate(new QDoubleSpinBox(this))
    , m_gain(new QDoubleSpinBox(this))
    , m_type(new QComboBox(this))
    , m_offset(new QDoubleSpinBox(this))
    , m_bandwidth(new QDoubleSpinBox(this))
    , m_filePath(new QLineEdit(this))
    , m_browseButton(new QPushButton(tr("…"), this))
    , m_controls(new QWidget(this))
    , m_startButton(new HazardButton(this))
    , m_spectrum(new SpectrumWidget(this))
    , m_waterfall(new WaterfallWidget(this))
{
    for (const Preset &p : kPresets)
        m_preset->addItem(QString::fromUtf8(p.name));

    m_frequency->setRange(0.1, 7500.0);
    m_frequency->setDecimals(3);
    m_frequency->setValue(433.92);
    m_frequency->setSuffix(tr(" MHz"));

    m_sampleRate->setRange(0.1, 61.44);
    m_sampleRate->setDecimals(3);
    m_sampleRate->setValue(2.0);
    m_sampleRate->setSuffix(tr(" MS/s"));

    m_gain->setRange(0.0, 60.0);
    m_gain->setDecimals(1);
    m_gain->setValue(30.0);
    m_gain->setSuffix(tr(" dB"));

    for (WaveformType t : kTypes)
        m_type->addItem(waveformName(t));

    // Signed offset from the carrier (− below, + above).
    m_offset->setRange(-30000.0, 30000.0);
    m_offset->setDecimals(1);
    m_offset->setValue(0.0);
    m_offset->setSuffix(tr(" kHz"));

    m_bandwidth->setRange(1.0, 61440.0);
    m_bandwidth->setDecimals(0);
    m_bandwidth->setValue(200.0);
    m_bandwidth->setSuffix(tr(" kHz"));

    m_filePath->setPlaceholderText(tr("IQ waveform file"));

    // Parameter form, wrapped in a scroll area so it never squeezes the viz.
    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(m_filePath, 1);
    fileRow->addWidget(m_browseButton);

    auto *form = new QFormLayout(m_controls);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("Preset"), m_preset);
    form->addRow(tr("Frequency"), m_frequency);
    form->addRow(tr("Sample rate"), m_sampleRate);
    form->addRow(tr("TX gain"), m_gain);
    form->addRow(tr("Waveform"), m_type);
    form->addRow(tr("Offset"), m_offset);
    form->addRow(tr("Bandwidth"), m_bandwidth);
    form->addRow(tr("File"), fileRow);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(m_controls);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_startButton->setText(tr("START"));
    m_startButton->setCheckable(true);
    // Always striped so it reads as a transmit trigger, running or not.
    m_startButton->setAlwaysHazard(true);

    m_spectrum->setMinimumHeight(64);
    m_waterfall->setMinimumHeight(64);

    // Controls on top, the hazard button pinned right above the always-visible
    // FFT + waterfall stack.
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(scroll, 1);
    layout->addWidget(m_startButton);
    layout->addWidget(m_spectrum, 1);
    layout->addWidget(m_waterfall, 1);

    // Preset fills the fields; any edit redraws the affected-band overlay.
    connect(m_preset, &QComboBox::currentIndexChanged, this,
            &JamProgram::applyPreset);
    connect(m_type, &QComboBox::currentIndexChanged, this,
            &JamProgram::onTypeChanged);
    connect(m_browseButton, &QPushButton::clicked, this, &JamProgram::browseFile);
    for (QDoubleSpinBox *box : {m_frequency, m_sampleRate, m_offset, m_bandwidth})
        connect(box, &QDoubleSpinBox::valueChanged, this,
                &JamProgram::updateOverlay);

    connect(m_startButton, &QPushButton::clicked, this, [this](bool checked) {
        if (checked)
            emit startRequested();
        else
            emit stopRequested();
    });

    onTypeChanged();
    updateOverlay();
}

void JamProgram::applyPreset(int index)
{
    if (index <= 0 || index >= static_cast<int>(kPresets.size()))
        return; // "Custom": leave the fields as-is
    const Preset &p = kPresets[index];
    m_frequency->setValue(p.frequencyMHz);
    m_sampleRate->setValue(p.sampleRateMSps);
    updateOverlay();
}

void JamProgram::onTypeChanged()
{
    const bool isFile = kTypes[m_type->currentIndex()] == WaveformType::IqFile;
    m_filePath->setEnabled(isFile);
    m_browseButton->setEnabled(isFile);
}

void JamProgram::browseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select IQ waveform"), QString(),
        tr("IQ files (*.iq *.C16 *.c16 *.cf32);;All files (*)"));
    if (!path.isEmpty())
        m_filePath->setText(path);
}

double JamProgram::affectedCenterHz() const
{
    return m_frequency->value() * 1e6 + m_offset->value() * 1e3;
}

double JamProgram::affectedWidthHz() const
{
    return m_bandwidth->value() * 1e3;
}

void JamProgram::updateOverlay()
{
    // Axis follows the tuned span; the shaded band is always drawn so the
    // affected bandwidth stays visible whether or not a transmission is live.
    m_spectrum->setAxis(m_frequency->value() * 1e6, m_sampleRate->value() * 1e6);
    m_spectrum->setCaptureRange(affectedCenterHz(), affectedWidthHz());
}

StreamParams JamProgram::streamParams() const
{
    StreamParams p;
    p.frequencyHz = m_frequency->value() * 1e6;
    p.sampleRateHz = m_sampleRate->value() * 1e6;
    p.gainDb = m_gain->value();
    return p;
}

WaveformConfig JamProgram::waveformConfig() const
{
    WaveformConfig w;
    w.type = kTypes[m_type->currentIndex()];
    w.amplitude = kAmplitude;
    w.offsetHz = m_offset->value() * 1e3;
    w.rangeHz = affectedWidthHz();
    w.filePath = m_filePath->text();
    return w;
}

void JamProgram::setRunning(bool running)
{
    QSignalBlocker block(m_startButton);
    m_startButton->setChecked(running);
    m_startButton->setText(running ? tr("STOP") : tr("START"));
    m_controls->setEnabled(!running);
    if (!running)
        m_waterfall->clear();
}

void JamProgram::pushSpectrum(const QVector<float> &row)
{
    m_spectrum->setTrace(row);
    m_waterfall->addRow(row);
}

} // namespace duality
