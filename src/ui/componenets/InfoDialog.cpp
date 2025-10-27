#include "InfoDialog.h"
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QDialogButtonBox>

InfoDialog::InfoDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Duality RF • Info");
  resize(720, 520);
  setStyleSheet(
      "background-color: black; color: #dddddd; font-family: monospace;"
      "QScrollArea { background-color: black; }"
      "QLabel { color: #dddddd; }"
      "QPushButton { background-color: #101010; color: #dddddd; border: 1px solid #888; padding: 4px 12px; }"
      "QPushButton:hover { background-color: #202020; }"
      "QPushButton:pressed { background-color: #303030; }");

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(12, 12, 12, 12);
  outer->setSpacing(8);

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *container = new QWidget(scroll);
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(12);

  auto *label = new QLabel(container);
  label->setTextFormat(Qt::RichText);
  label->setWordWrap(true);

  const char *html = R"(
  <div style='color:#dddddd;'>
  <h2 style='color:#ffffff;font-weight:bold;margin:0 0 8px 0;'>Duality RF Console — Reference</h2>
  <p>This window summarizes the key controls and processing blocks used by the receiver UI.</p>

  <h3 style='color:cyan;'>Display</h3>
  <ul>
    <li><b>Spectrum</b>: Cyan trace (live) with orange peak-hold. Units are dB relative to full-scale.</li>
    <li><b>Waterfall</b>: Time vs. frequency intensity map. RX (yellow) and TX (red) guides show reference frequencies.</li>
    <li><b>Zoom</b>: 1x–16x. Affects both spectrum and waterfall views.</li>
  </ul>

  <h3 style='color:cyan;'>RF Settings</h3>
  <ul>
    <li><b>RX Frequency</b>: Center tuning in MHz.</li>
    <li><b>Sample Rate</b>: ADC sampling rate. Impacts FFT span and detector timing (samples = seconds × rate).</li>
    <li><b>Gain</b>: Manual RTL-SDR gain in dB (AGC disabled).</li>
  </ul>

  <h3 style='color:cyan;'>Triggered Capture</h3>
  <ul>
    <li><b>Capture Threshold</b>: Horizontal yellow line in spectrum; measurements above it arm/trigger capture.</li>
    <li><b>Capture Span</b>: ±Hz window around RX used for detection. Peaks outside this green band are ignored.</li>
    <li><b>Detector</b>:
      <ul>
        <li><b>Averaged</b>: Exponential average with time constant <b>Avg Tau</b> (τ) is compared to threshold.</li>
        <li><b>Peak</b>: Instantaneous amplitude; τ is ignored for detection. A single above-threshold FFT block is enough.</li>
      </ul>
    </li>
    <li><b>Dwell</b>: Minimum time the signal must remain above threshold before capture starts (Averaged mode). In Peak mode we effectively require one block.</li>
    <li><b>Avg Tau</b> (τ): Time constant for averaging in Averaged mode; larger τ = smoother but slower response.</li>
    <li><b>Status</b>: Shows Armed/Captured and Above/Below with live center/threshold dB readout.</li>
  </ul>

  <h3 style='color:cyan;'>Files & Storage</h3>
  <ul>
    <li>While Armed, raw complex samples are spooled to a temporary <code>captures/in_progress_*.cf32.part</code> file for visibility.</li>
    <li>On capture completion, a trimmed <code>.cf32</code> file is written to the <code>captures/</code> folder. Names include RX MHz and threshold.</li>
  </ul>

  <h3 style='color:cyan;'>Tips</h3>
  <ul>
    <li>If a visible peak does not trigger, increase the Capture Span or retune RX so the peak sits inside the green band.</li>
    <li>Use <b>Peak</b> detector for short packets; use <b>Averaged</b> with a larger τ and some Dwell to suppress noise.</li>
    <li>Lowering the Threshold increases sensitivity; raising it reduces false triggers.</li>
  </ul>
  </div>
  )";

  label->setText(html);
  layout->addWidget(label);
  layout->addStretch(1);

  scroll->setWidget(container);
  outer->addWidget(scroll, 1);

  // Close button row
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  outer->addWidget(buttons);
}
