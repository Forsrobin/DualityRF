#include "ui/panels/TransmitPanel.h"

#include "ui/components/HazardButton.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace duality {

namespace {
const WaveformType kTypes[] = {
    WaveformType::WhiteNoise,   WaveformType::GaussianNoise,
    WaveformType::ContinuousWave, WaveformType::Sine,
    WaveformType::IqFile,
};
}

TransmitPanel::TransmitPanel(QWidget *parent)
    : QWidget(parent)
    , m_group(new QGroupBox(this))
    , m_enableButton(new HazardButton(this))
    , m_type(new QComboBox(this))
    , m_amplitude(new QDoubleSpinBox(this))
    , m_offsetSign(new QPushButton(QStringLiteral("+"), this))
    , m_offset(new QDoubleSpinBox(this))
    , m_range(new QDoubleSpinBox(this))
    , m_filePath(new QLineEdit(this))
    , m_browseButton(new QPushButton(tr("…"), this))
    , m_gain(new QDoubleSpinBox(this))
{
    // Enable/disable is a toggle button whose label reflects the state.
    m_enableButton->setCheckable(true);
    m_enableButton->setChecked(false);
    m_enableButton->setText(tr("DISABLED"));

    for (WaveformType t : kTypes)
        m_type->addItem(waveformName(t));

    m_amplitude->setRange(0.0, 1.0);
    m_amplitude->setSingleStep(0.05);
    m_amplitude->setDecimals(2);
    m_amplitude->setValue(0.5);

    // Direction switch: checked = below the carrier (−), unchecked = above.
    m_offsetSign->setCheckable(true);
    m_offsetSign->setFixedWidth(52);
    connect(m_offsetSign, &QPushButton::toggled, this, [this](bool minus) {
        m_offsetSign->setText(minus ? QStringLiteral("−")
                                    : QStringLiteral("+"));
    });

    m_offset->setRange(0.0, 30000.0);
    m_offset->setDecimals(1);
    m_offset->setValue(100.0);
    m_offset->setSuffix(tr(" kHz"));

    m_range->setRange(0.0, 30000.0);
    m_range->setDecimals(1);
    m_range->setValue(50.0);
    m_range->setSuffix(tr(" kHz"));
    m_range->setSpecialValueText(tr("full band"));

    m_filePath->setPlaceholderText(tr("IQ waveform file"));

    m_gain->setRange(0.0, 60.0);
    m_gain->setDecimals(1);
    m_gain->setValue(20.0);
    m_gain->setSuffix(tr(" dB"));

    auto *fileRow = new QHBoxLayout;
    fileRow->addWidget(m_filePath, 1);
    fileRow->addWidget(m_browseButton);

    auto *offsetRow = new QHBoxLayout;
    offsetRow->addWidget(m_offsetSign);
    offsetRow->addWidget(m_offset, 1);

    auto *form = new QFormLayout(m_group);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(m_enableButton);
    form->addRow(tr("Waveform"), m_type);
    form->addRow(tr("Amplitude"), m_amplitude);
    form->addRow(tr("Offset"), offsetRow);
    form->addRow(tr("Range"), m_range);
    form->addRow(tr("File"), fileRow);
    form->addRow(tr("TX gain"), m_gain);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_group);
    // Keep the group box at its natural height (top-anchored) instead of
    // stretching to fill the scroll viewport, which would leave it with dead
    // space and nothing to scroll.
    layout->addStretch(1);

    connect(m_type, &QComboBox::currentIndexChanged, this,
            &TransmitPanel::onTypeChanged);
    connect(m_browseButton, &QPushButton::clicked, this,
            &TransmitPanel::browseFile);
    connect(m_enableButton, &QPushButton::toggled, this, [this](bool on) {
        m_enableButton->setText(on ? tr("ENABLED") : tr("DISABLED"));
        emit enabledChanged(on);
    });

    // Any waveform-parameter edit re-emits waveformChanged so a live
    // transmission can swap to the new waveform.
    connect(m_type, &QComboBox::currentIndexChanged, this,
            &TransmitPanel::waveformChanged);
    connect(m_offsetSign, &QPushButton::toggled, this,
            &TransmitPanel::waveformChanged);
    connect(m_filePath, &QLineEdit::textChanged, this,
            &TransmitPanel::waveformChanged);
    for (QDoubleSpinBox *box : {m_amplitude, m_offset, m_range})
        connect(box, &QDoubleSpinBox::valueChanged, this,
                &TransmitPanel::waveformChanged);

    onTypeChanged();
}

void TransmitPanel::onTypeChanged()
{
    const WaveformType t = kTypes[m_type->currentIndex()];
    // Every generator is shifted by the offset; only the noise generators
    // are band-limited by the range.
    m_range->setEnabled(t == WaveformType::WhiteNoise ||
                        t == WaveformType::GaussianNoise);
    m_filePath->setEnabled(t == WaveformType::IqFile);
    m_browseButton->setEnabled(t == WaveformType::IqFile);
}

void TransmitPanel::browseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select IQ waveform"), QString(),
        tr("IQ files (*.iq *.C16 *.c16 *.cf32);;All files (*)"));
    if (!path.isEmpty())
        m_filePath->setText(path);
}

bool TransmitPanel::transmitEnabled() const
{
    return m_enableButton->isChecked();
}

void TransmitPanel::setTransmitEnabled(bool enabled)
{
    m_enableButton->setChecked(enabled);
}

WaveformConfig TransmitPanel::waveformConfig() const
{
    WaveformConfig cfg;
    cfg.type = kTypes[m_type->currentIndex()];
    cfg.amplitude = m_amplitude->value();
    cfg.offsetHz =
        (m_offsetSign->isChecked() ? -1.0 : 1.0) * m_offset->value() * 1e3;
    cfg.rangeHz = m_range->isEnabled() ? m_range->value() * 1e3 : 0.0;
    cfg.filePath = m_filePath->text();
    return cfg;
}

double TransmitPanel::txGainDb() const
{
    return m_gain->value();
}

} // namespace duality
