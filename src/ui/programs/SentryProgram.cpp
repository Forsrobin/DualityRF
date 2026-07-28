#include "ui/programs/SentryProgram.h"

#include "ui/components/HazardButton.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/widgets/WaterfallWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>

namespace duality {

namespace {
// Bands worth guarding: name, carrier (MHz), span (MS/s). Slanted toward the
// ISM bands where remotes, fobs and sensors live. The first entry is a
// free-form "Custom" that leaves the fields untouched.
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

// Jam waveform choices, matching the JAM program.
const WaveformType kTypes[] = {
    WaveformType::WhiteNoise,     WaveformType::GaussianNoise,
    WaveformType::ContinuousWave, WaveformType::Sine,
    WaveformType::IqFile,
};

// Fixed amplitude: the TX gain is the only level control in this program.
constexpr double kAmplitude = 0.7;
} // namespace

SentryProgram::SentryProgram(QWidget *parent)
    : QWidget(parent), m_preset(new QComboBox(this)),
      m_frequency(new QDoubleSpinBox(this)),
      m_sampleRate(new QDoubleSpinBox(this)),
      m_watchOffset(new QDoubleSpinBox(this)),
      m_watchWidth(new QDoubleSpinBox(this)), m_threshold(new QSpinBox(this)),
      m_type(new QComboBox(this)), m_gain(new QDoubleSpinBox(this)),
      m_filePath(new QLineEdit(this)), m_browseButton(new QPushButton("…", this)),
      m_burst(new QSpinBox(this)), m_cooldown(new QSpinBox(this)),
      m_status(new QLabel(this)), m_controls(new QWidget(this)),
      m_startButton(new HazardButton(this)),
      m_spectrum(new SpectrumWidget(this)),
      m_waterfall(new WaterfallWidget(this)) {
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

  // Watch window: a signed offset from the carrier and a width. Detection only
  // looks inside this window so out-of-band energy never trips the jammer.
  m_watchOffset->setRange(-30000.0, 30000.0);
  m_watchOffset->setDecimals(1);
  m_watchOffset->setValue(0.0);
  m_watchOffset->setSuffix(tr(" kHz"));

  m_watchWidth->setRange(1.0, 61440.0);
  m_watchWidth->setDecimals(0);
  m_watchWidth->setValue(400.0);
  m_watchWidth->setSuffix(tr(" kHz"));

  // Trigger level in the same dB scale the FFT trace shows, so the operator can
  // read a quiet-band floor off the waterfall and set the threshold above it.
  m_threshold->setRange(-120, 20);
  m_threshold->setValue(-40);
  m_threshold->setSuffix(tr(" dB"));

  for (WaveformType t : kTypes)
    m_type->addItem(waveformName(t));

  m_gain->setRange(0.0, 60.0);
  m_gain->setDecimals(1);
  m_gain->setValue(30.0);
  m_gain->setSuffix(tr(" dB"));

  m_filePath->setPlaceholderText(tr("IQ waveform file"));

  m_burst->setRange(10, 10000);
  m_burst->setValue(300);
  m_burst->setSingleStep(50);
  m_burst->setSuffix(tr(" ms"));

  m_cooldown->setRange(0, 10000);
  m_cooldown->setValue(200);
  m_cooldown->setSingleStep(50);
  m_cooldown->setSuffix(tr(" ms"));

  m_status->setAlignment(Qt::AlignCenter);
  m_status->setObjectName(QStringLiteral("sentryStatus"));

  auto *fileRow = new QHBoxLayout;
  fileRow->addWidget(m_filePath, 1);
  fileRow->addWidget(m_browseButton);

  auto *form = new QFormLayout(m_controls);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->addRow(tr("Preset"), m_preset);
  form->addRow(tr("Frequency"), m_frequency);
  form->addRow(tr("Sample rate"), m_sampleRate);
  form->addRow(tr("Watch offset"), m_watchOffset);
  form->addRow(tr("Watch width"), m_watchWidth);
  form->addRow(tr("Threshold"), m_threshold);
  form->addRow(tr("Jam waveform"), m_type);
  form->addRow(tr("TX gain"), m_gain);
  form->addRow(tr("File"), fileRow);
  form->addRow(tr("Burst"), m_burst);
  form->addRow(tr("Cooldown"), m_cooldown);

  auto *scroll = new QScrollArea(this);
  scroll->setWidget(m_controls);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_startButton->setText(tr("ARM"));
  m_startButton->setCheckable(true);
  // Always striped so it reads as a transmit trigger, armed or not.
  m_startButton->setAlwaysHazard(true);

  m_spectrum->setMinimumHeight(64);
  m_waterfall->setMinimumHeight(64);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(scroll, 1);
  layout->addWidget(m_status);
  layout->addWidget(m_startButton);
  layout->addWidget(m_spectrum, 1);
  layout->addWidget(m_waterfall, 1);

  connect(m_preset, &QComboBox::currentIndexChanged, this,
          &SentryProgram::applyPreset);
  connect(m_type, &QComboBox::currentIndexChanged, this,
          &SentryProgram::onTypeChanged);
  connect(m_browseButton, &QPushButton::clicked, this,
          &SentryProgram::browseFile);
  for (QDoubleSpinBox *box : {m_frequency, m_sampleRate, m_watchOffset,
                              m_watchWidth})
    connect(box, &QDoubleSpinBox::valueChanged, this,
            &SentryProgram::updateOverlay);

  connect(m_startButton, &QPushButton::clicked, this, [this](bool checked) {
    if (checked)
      emit startRequested();
    else
      emit stopRequested();
  });

  onTypeChanged();
  updateOverlay();
  updateStatusLabel();
}

QString SentryProgram::infoText() {
  return tr(
      "<p>The <b>Sentry</b> is a <b>reactive (triggered) jammer</b>. It watches "
      "the receiver for a transmission in a chosen window and, the instant the "
      "in-band peak crosses the <b>threshold</b>, fires a short jamming "
      "<b>burst</b> on the transmitter, then holds off for a <b>cooldown</b> "
      "before re-arming. Unlike <b>JAM</b>, which radiates continuously, the "
      "Sentry only transmits in response to a detected signal.</p>"
      "<p><b>How to use</b></p>"
      "<ol>"
      "<li>In <b>DEVICEES</b>, select both a <b>receiver</b> (to detect) and a "
      "<b>transmitter</b> (to jam). Two separate devices are recommended so it "
      "can listen while it transmits.</li>"
      "<li><b>Preset / Frequency / Sample rate</b> — tune the channel to "
      "guard.</li>"
      "<li><b>Watch offset / width</b> — the window inside the span that "
      "detection looks at (shaded on the FFT). Energy outside it is ignored.</li>"
      "<li><b>Threshold</b> — the trigger level, in the same dB scale as the "
      "trace. Watch the quiet-band floor, then set this a little above it.</li>"
      "<li><b>Jam waveform / TX gain</b> — what the burst transmits and how "
      "hard.</li>"
      "<li><b>Burst / Cooldown</b> — how long each burst lasts and the "
      "hold-off after it before the next trigger can fire.</li>"
      "<li>Press <b>ARM</b>. The readout shows a running count of bursts "
      "fired.</li>"
      "</ol>"
      "<p><b>Notes</b><br>"
      "Detection pauses during a burst and its cooldown, so the jammer never "
      "re-triggers on its own signal. Leaving this screen automatically "
      "disarms, so it never runs unseen. Only transmit on frequencies you are "
      "licensed and authorized to use.</p>");
}

void SentryProgram::applyPreset(int index) {
  if (index <= 0 || index >= static_cast<int>(kPresets.size()))
    return; // "Custom": leave the fields as-is
  const Preset &p = kPresets[index];
  m_frequency->setValue(p.frequencyMHz);
  m_sampleRate->setValue(p.sampleRateMSps);
  updateOverlay();
}

void SentryProgram::onTypeChanged() {
  const bool isFile = kTypes[m_type->currentIndex()] == WaveformType::IqFile;
  m_filePath->setEnabled(isFile);
  m_browseButton->setEnabled(isFile);
}

void SentryProgram::browseFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Select IQ waveform"), QString(),
      tr("IQ files (*.iq *.C16 *.c16 *.cf32);;All files (*)"));
  if (!path.isEmpty())
    m_filePath->setText(path);
}

double SentryProgram::watchCenterHz() const {
  return m_frequency->value() * 1e6 + m_watchOffset->value() * 1e3;
}

double SentryProgram::watchWidthHz() const {
  return m_watchWidth->value() * 1e3;
}

void SentryProgram::updateOverlay() {
  // Axis follows the tuned span; the shaded band shows the watch window so the
  // operator can see exactly where detection is looking.
  m_spectrum->setAxis(m_frequency->value() * 1e6, m_sampleRate->value() * 1e6);
  m_spectrum->setCaptureRange(watchCenterHz(), watchWidthHz());
}

void SentryProgram::updateStatusLabel() {
  QString state;
  if (!m_armed)
    state = tr("IDLE");
  else if (m_firing)
    state = tr("JAMMING");
  else
    state = tr("WATCHING");
  m_status->setText(tr("%1 — %2 bursts").arg(state).arg(m_triggerCount));
}

StreamParams SentryProgram::streamParams() const {
  StreamParams p;
  p.frequencyHz = m_frequency->value() * 1e6;
  p.sampleRateHz = m_sampleRate->value() * 1e6;
  p.gainDb = m_gain->value();
  return p;
}

WaveformConfig SentryProgram::waveformConfig() const {
  WaveformConfig w;
  w.type = kTypes[m_type->currentIndex()];
  w.amplitude = kAmplitude;
  w.offsetHz = m_watchOffset->value() * 1e3;
  w.rangeHz = watchWidthHz();
  w.filePath = m_filePath->text();
  return w;
}

float SentryProgram::thresholdDb() const {
  return static_cast<float>(m_threshold->value());
}

int SentryProgram::burstMs() const { return m_burst->value(); }

int SentryProgram::cooldownMs() const { return m_cooldown->value(); }

void SentryProgram::setRunning(bool running) {
  QSignalBlocker block(m_startButton);
  m_armed = running;
  m_startButton->setChecked(running);
  m_startButton->setText(running ? tr("DISARM") : tr("ARM"));
  m_controls->setEnabled(!running);
  if (!running) {
    m_firing = false;
    m_waterfall->clear();
  }
  updateStatusLabel();
}

void SentryProgram::setTriggerCount(int count) {
  m_triggerCount = count;
  updateStatusLabel();
}

void SentryProgram::setFiring(bool firing) {
  m_firing = firing;
  updateStatusLabel();
}

void SentryProgram::pushSpectrum(const QVector<float> &row) {
  m_spectrum->setTrace(row);
  m_waterfall->addRow(row);
}

} // namespace duality
