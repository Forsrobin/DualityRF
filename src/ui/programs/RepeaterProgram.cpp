#include "ui/programs/RepeaterProgram.h"

#include "ui/components/HazardButton.h"
#include "ui/widgets/SpectrumWidget.h"
#include "ui/widgets/WaterfallWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>

namespace duality {

namespace {
// Bands worth repeating: name, carrier (MHz), span (MS/s). Slanted toward the
// ISM bands where short-range remotes and sensors live. The first entry is a
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
} // namespace

RepeaterProgram::RepeaterProgram(QWidget *parent)
    : QWidget(parent), m_preset(new QComboBox(this)),
      m_frequency(new QDoubleSpinBox(this)),
      m_sampleRate(new QDoubleSpinBox(this)), m_rxGain(new QDoubleSpinBox(this)),
      m_threshold(new QSpinBox(this)),
      m_captureRange(new QDoubleSpinBox(this)),
      m_offset(new QDoubleSpinBox(this)),
      m_txGain(new QDoubleSpinBox(this)), m_status(new QLabel(this)),
      m_controls(new QWidget(this)), m_startButton(new HazardButton(this)),
      m_spectrum(new SpectrumWidget(this)),
      m_waterfall(new WaterfallWidget(this)) {
  for (const Preset &p : kPresets)
    m_preset->addItem(QString::fromUtf8(p.name));

  m_frequency->setRange(0.1, 7500.0);
  m_frequency->setDecimals(3);
  m_frequency->setValue(433.810);
  m_frequency->setSuffix(tr(" MHz"));

  m_sampleRate->setRange(0.1, 61.44);
  m_sampleRate->setDecimals(3);
  m_sampleRate->setValue(2.0);
  m_sampleRate->setSuffix(tr(" MS/s"));

  m_rxGain->setRange(0.0, 60.0);
  m_rxGain->setDecimals(1);
  m_rxGain->setValue(30.0);
  m_rxGain->setSuffix(tr(" dB"));

  // Trigger level for the time-domain power detector, in dBFS.
  m_threshold->setRange(-120, 0);
  m_threshold->setValue(-50);
  m_threshold->setSuffix(tr(" dBFS"));

  // ± band the captured burst is limited to before replay. 0 replays the whole
  // received span.
  m_captureRange->setRange(0.0, 30000.0);
  m_captureRange->setDecimals(1);
  m_captureRange->setValue(100.0);
  m_captureRange->setSuffix(tr(" kHz"));

  // Retransmit offset from the receive carrier. 0 repeats on the same
  // frequency; a non-zero value rebroadcasts the burst shifted by this much.
  m_offset->setRange(-50000.0, 50000.0);
  m_offset->setDecimals(1);
  m_offset->setValue(0.0);
  m_offset->setSuffix(tr(" kHz"));

  m_txGain->setRange(0.0, 60.0);
  m_txGain->setDecimals(1);
  m_txGain->setValue(30.0);
  m_txGain->setSuffix(tr(" dB"));

  m_status->setAlignment(Qt::AlignCenter);
  m_status->setObjectName(QStringLiteral("repeaterStatus"));

  auto *form = new QFormLayout(m_controls);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->addRow(tr("Preset"), m_preset);
  form->addRow(tr("Frequency"), m_frequency);
  form->addRow(tr("Sample rate"), m_sampleRate);
  form->addRow(tr("RX gain"), m_rxGain);
  form->addRow(tr("Threshold"), m_threshold);
  form->addRow(tr("Capture range"), m_captureRange);
  form->addRow(tr("TX offset"), m_offset);
  form->addRow(tr("TX gain"), m_txGain);

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
          &RepeaterProgram::applyPreset);
  for (QDoubleSpinBox *box : {m_frequency, m_sampleRate, m_offset, m_captureRange})
    connect(box, &QDoubleSpinBox::valueChanged, this,
            &RepeaterProgram::updateOverlay);

  connect(m_startButton, &QPushButton::clicked, this, [this](bool checked) {
    if (checked)
      emit startRequested();
    else
      emit stopRequested();
  });

  updateOverlay();
  updateStatusLabel();
}

QString RepeaterProgram::infoText() {
  return tr(
      "<p>The <b>Repeater</b> is a <b>store-and-forward relay</b>. It listens "
      "on a channel and, whenever it detects a transmission, buffers that "
      "burst in memory and immediately retransmits it once — so a weak signal "
      "is picked up and rebroadcast to reach further.</p>"
      "<p><b>How to use</b></p>"
      "<ol>"
      "<li>In <b>DEVICEES</b>, select a <b>receiver</b> and a "
      "<b>transmitter</b>. The receiver keeps running while it retransmits, so "
      "the FFT/waterfall show the replayed signal live. This needs a radio that "
      "can receive and transmit at once — a <b>full-duplex device</b>, or "
      "<b>two separate devices</b>. On a strictly half-duplex radio the "
      "retransmit fails and a toast reports it.</li>"
      "<li><b>Preset / Frequency / Sample rate</b> — tune the channel to "
      "repeat.</li>"
      "<li><b>RX gain</b> — receive gain; raise it until faint bursts show on "
      "the waterfall.</li>"
      "<li><b>Threshold</b> — the detector's trigger level (dBFS). Set it just "
      "above the idle-band floor so noise does not trip it. Detected signals "
      "are marked with a peak line on the trace and announced as a toast.</li>"
      "<li><b>Capture range</b> — band-limits the captured burst to ± this much "
      "around the receive frequency before it is replayed (shaded on the "
      "trace), so only that slice of the band is rebroadcast. 0 replays the "
      "whole span.</li>"
      "<li><b>TX offset</b> — how far to shift the retransmit carrier. "
      "<b>0</b> repeats on the same frequency; a non-zero value rebroadcasts "
      "the burst on an offset frequency.</li>"
      "<li><b>TX gain</b> — retransmit output level.</li>"
      "<li>Press <b>ARM</b>. The readout cycles LISTENING → CAPTURING → "
      "REPEATING and counts each burst relayed.</li>"
      "</ol>"
      "<p><b>Notes</b><br>"
      "Because it stores then forwards, the repeat is delayed by roughly the "
      "length of the captured burst; it is not a live analog repeater. Leaving "
      "this screen automatically disarms, so it never transmits unseen. Only "
      "transmit on frequencies you are licensed and authorized to use.</p>");
}

void RepeaterProgram::applyPreset(int index) {
  if (index <= 0 || index >= static_cast<int>(kPresets.size()))
    return; // "Custom": leave the fields as-is
  const Preset &p = kPresets[index];
  m_frequency->setValue(p.frequencyMHz);
  m_sampleRate->setValue(p.sampleRateMSps);
  updateOverlay();
}

void RepeaterProgram::updateOverlay() {
  // The trace axis follows the receive span; the shaded band shows the capture
  // range — the slice of the band that will actually be replayed (centered on
  // the receive frequency). 0 shades nothing, i.e. the whole span is replayed.
  m_spectrum->setAxis(m_frequency->value() * 1e6, m_sampleRate->value() * 1e6);
  m_spectrum->setCaptureRange(m_frequency->value() * 1e6, 2.0 * captureRangeHz());
}

void RepeaterProgram::updateStatusLabel() {
  QString state;
  switch (m_phase) {
  case Phase::Idle:
    state = tr("IDLE");
    break;
  case Phase::Listening:
    state = tr("LISTENING");
    break;
  case Phase::Capturing:
    state = tr("CAPTURING");
    break;
  case Phase::Transmitting:
    state = tr("REPEATING");
    break;
  }
  m_status->setText(tr("%1 — %2 repeats").arg(state).arg(m_repeatCount));
}

StreamParams RepeaterProgram::rxParams() const {
  StreamParams p;
  p.frequencyHz = m_frequency->value() * 1e6;
  p.sampleRateHz = m_sampleRate->value() * 1e6;
  p.gainDb = m_rxGain->value();
  return p;
}

double RepeaterProgram::offsetHz() const { return m_offset->value() * 1e3; }

double RepeaterProgram::captureRangeHz() const {
  return m_captureRange->value() * 1e3;
}

double RepeaterProgram::txGainDb() const { return m_txGain->value(); }

float RepeaterProgram::thresholdDb() const {
  return static_cast<float>(m_threshold->value());
}

void RepeaterProgram::setPhase(Phase phase) {
  m_phase = phase;
  updateStatusLabel();
}

void RepeaterProgram::setRepeatCount(int count) {
  m_repeatCount = count;
  updateStatusLabel();
}

void RepeaterProgram::setRunning(bool running) {
  QSignalBlocker block(m_startButton);
  m_startButton->setChecked(running);
  m_startButton->setText(running ? tr("DISARM") : tr("ARM"));
  m_controls->setEnabled(!running);
  if (!running) {
    m_phase = Phase::Idle;
    m_waterfall->clear();
    m_spectrum->setMarkers({});
  }
  updateStatusLabel();
}

void RepeaterProgram::pushSpectrum(const QVector<float> &row) {
  m_spectrum->setTrace(row);
  m_waterfall->addRow(row);
}

void RepeaterProgram::setPeakMarkers(const QVector<SpectrumMarker> &markers) {
  m_spectrum->setMarkers(markers);
}

} // namespace duality
