#include "ui/programs/BeaconProgram.h"

#include "ui/components/HazardButton.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/widgets/WaterfallWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace duality {

namespace {
// Common CW beacon spots: name, carrier (MHz), span (MS/s). Slanted toward the
// amateur bands where beacons live, plus the 433 MHz ISM band for bench tests.
// The first entry is a free-form "Custom" that leaves the fields untouched.
struct Preset {
  const char *name;
  double frequencyMHz;
  double sampleRateMSps;
};
constexpr std::array<Preset, 7> kPresets{{
    {"Custom", 0.0, 0.0},
    {"28.2 MHz (10m beacon)", 28.2, 2.0},
    {"50.4 MHz (6m beacon)", 50.4, 2.0},
    {"144.4 MHz (2m beacon)", 144.4, 2.0},
    {"432.3 MHz (70cm beacon)", 432.3, 2.0},
    {"433.92 MHz (ISM)", 433.92, 2.0},
    {"1296.5 MHz (23cm beacon)", 1296.5, 2.0},
}};

// Fixed amplitude: the TX gain is the only level control in this program.
constexpr double kAmplitude = 0.7;
} // namespace

BeaconProgram::BeaconProgram(QWidget *parent)
    : QWidget(parent), m_preset(new QComboBox(this)),
      m_message(new QLineEdit(this)), m_frequency(new QDoubleSpinBox(this)),
      m_sampleRate(new QDoubleSpinBox(this)), m_tone(new QDoubleSpinBox(this)),
      m_wpm(new QSpinBox(this)), m_gain(new QDoubleSpinBox(this)),
      m_preview(new QLabel(this)), m_controls(new QWidget(this)),
      m_startButton(new HazardButton(this)),
      m_spectrum(new SpectrumWidget(this)),
      m_waterfall(new WaterfallWidget(this)) {
  for (const Preset &p : kPresets)
    m_preset->addItem(QString::fromUtf8(p.name));

  m_message->setText(tr("CQ CQ DE DUALITY"));
  m_message->setMaxLength(120);
  m_message->setPlaceholderText(tr("Beacon message"));

  m_frequency->setRange(0.1, 7500.0);
  m_frequency->setDecimals(3);
  m_frequency->setValue(144.4);
  m_frequency->setSuffix(tr(" MHz"));

  m_sampleRate->setRange(0.1, 61.44);
  m_sampleRate->setDecimals(3);
  m_sampleRate->setValue(2.0);
  m_sampleRate->setSuffix(tr(" MS/s"));

  // CW pitch above the carrier. A single complex tone, keyed on/off.
  m_tone->setRange(0.05, 20.0);
  m_tone->setDecimals(2);
  m_tone->setValue(0.7);
  m_tone->setSuffix(tr(" kHz"));

  m_wpm->setRange(5, 60);
  m_wpm->setValue(20);
  m_wpm->setSuffix(tr(" wpm"));

  m_gain->setRange(0.0, 60.0);
  m_gain->setDecimals(1);
  m_gain->setValue(20.0);
  m_gain->setSuffix(tr(" dB"));

  m_preview->setWordWrap(true);
  m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto *form = new QFormLayout(m_controls);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->addRow(tr("Message"), m_message);
  form->addRow(tr("Preset"), m_preset);
  form->addRow(tr("Frequency"), m_frequency);
  form->addRow(tr("Sample rate"), m_sampleRate);
  form->addRow(tr("Tone"), m_tone);
  form->addRow(tr("Speed"), m_wpm);
  form->addRow(tr("TX gain"), m_gain);
  form->addRow(tr("Morse"), m_preview);

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

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(scroll, 1);
  layout->addWidget(m_startButton);
  layout->addWidget(m_spectrum, 1);
  layout->addWidget(m_waterfall, 1);

  connect(m_preset, &QComboBox::currentIndexChanged, this,
          &BeaconProgram::applyPreset);
  connect(m_message, &QLineEdit::textChanged, this,
          &BeaconProgram::updateOverlay);
  for (QDoubleSpinBox *box : {m_frequency, m_sampleRate, m_tone})
    connect(box, &QDoubleSpinBox::valueChanged, this,
            &BeaconProgram::updateOverlay);
  connect(m_wpm, &QSpinBox::valueChanged, this, &BeaconProgram::updateOverlay);

  connect(m_startButton, &QPushButton::clicked, this, [this](bool checked) {
    if (checked)
      emit startRequested();
    else
      emit stopRequested();
  });

  updateOverlay();
}

QString BeaconProgram::infoText() {
  return tr(
      "<p>The <b>Beacon</b> keys a text message as looping "
      "<b>International Morse</b> code (CW). Use it as a station-ID beacon, a "
      "fox-hunt / ARDF transmitter, or a known signal for antenna testing.</p>"
      "<p><b>How to use</b></p>"
      "<ol>"
      "<li>In <b>DEVICEES</b>, select a <b>transmitter</b>. Optionally select a "
      "<b>receiver</b> too, to watch the keyed signal live on the "
      "FFT/waterfall.</li>"
      "<li><b>Message</b> — the text to key. Letters, digits and "
      "<tt>. , ? / = + - @</tt> are supported; other characters are skipped. "
      "The <b>Morse</b> line previews the dits/dahs.</li>"
      "<li><b>Preset / Frequency / Sample rate</b> — pick the band. Presets "
      "cover the common amateur beacon subbands plus 433 MHz ISM.</li>"
      "<li><b>Tone</b> — the CW pitch above the carrier.</li>"
      "<li><b>Speed</b> — keying speed in words per minute (PARIS standard).</li>"
      "<li><b>TX gain</b> — transmitter output level.</li>"
      "<li>Press <b>START</b>. The message loops with a word gap between "
      "repeats until you press <b>STOP</b>.</li>"
      "</ol>"
      "<p><b>Notes</b><br>"
      "Keyed elements are shaped with a raised-cosine rise/fall so the "
      "transmitted signal has no key clicks. Leaving this screen automatically "
      "stops the beacon, so it never transmits unseen. Only transmit on "
      "frequencies you are licensed and authorized to use.</p>");
}

void BeaconProgram::applyPreset(int index) {
  if (index <= 0 || index >= static_cast<int>(kPresets.size()))
    return; // "Custom": leave the fields as-is
  const Preset &p = kPresets[index];
  m_frequency->setValue(p.frequencyMHz);
  m_sampleRate->setValue(p.sampleRateMSps);
  updateOverlay();
}

double BeaconProgram::toneCenterHz() const {
  return m_frequency->value() * 1e6 + m_tone->value() * 1e3;
}

double BeaconProgram::occupiedWidthHz() const {
  // Keyed CW occupies roughly four times the keying rate around the tone; the
  // keying rate is WPM/1.2 baud (PARIS). Floored so the band stays visible on
  // a wide span even though the true footprint is only tens of hertz.
  const double keyingRate = m_wpm->value() / 1.2;
  return std::max(4.0 * keyingRate, m_sampleRate->value() * 1e6 * 0.02);
}

void BeaconProgram::updateOverlay() {
  m_spectrum->setAxis(m_frequency->value() * 1e6, m_sampleRate->value() * 1e6);
  m_spectrum->setCaptureRange(toneCenterHz(), occupiedWidthHz());

  const QString code = morsePreview(m_message->text());
  m_preview->setText(code.isEmpty() ? tr("—") : code);
}

StreamParams BeaconProgram::streamParams() const {
  StreamParams p;
  p.frequencyHz = m_frequency->value() * 1e6;
  p.sampleRateHz = m_sampleRate->value() * 1e6;
  p.gainDb = m_gain->value();
  return p;
}

WaveformConfig BeaconProgram::waveformConfig() const {
  WaveformConfig w;
  w.type = WaveformType::Morse;
  w.amplitude = kAmplitude;
  w.offsetHz = m_tone->value() * 1e3;
  w.text = m_message->text();
  w.wpm = m_wpm->value();
  return w;
}

void BeaconProgram::setRunning(bool running) {
  QSignalBlocker block(m_startButton);
  m_startButton->setChecked(running);
  m_startButton->setText(running ? tr("STOP") : tr("START"));
  m_controls->setEnabled(!running);
  if (!running)
    m_waterfall->clear();
}

void BeaconProgram::pushSpectrum(const QVector<float> &row) {
  m_spectrum->setTrace(row);
  m_waterfall->addRow(row);
}

} // namespace duality
