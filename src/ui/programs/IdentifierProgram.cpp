#include "ui/programs/IdentifierProgram.h"

#include <QComboBox>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>

namespace duality {

namespace {
// Common bands worth identifying: name, carrier (MHz), span (MS/s). Reused from
// the JAM/SENTRY presets and slanted toward the ISM bands where remotes, fobs
// and sensors live. Unlike those programs there is no free-form "Custom" — the
// scan always tunes a concrete band.
struct Band {
  const char *name;
  double frequencyMHz;
  double sampleRateMSps;
};
constexpr std::array<Band, 5> kBands{{
    {"315 MHz (ISM)", 315.0, 2.0},
    {"433.92 MHz (ISM)", 433.92, 2.0},
    {"868 MHz (ISM)", 868.0, 2.0},
    {"915 MHz (ISM)", 915.0, 2.0},
    {"2450 MHz (ISM)", 2450.0, 10.0},
}};
} // namespace

// A pure eye-candy "radar" widget: concentric rings pulse outward from the
// centre while a sweep line rotates over them, in the app's monochrome pixel
// look. Runs a repaint timer only while active so an idle screen costs nothing.
// No Q_OBJECT: it declares no signals/slots (the timer drives update() through
// a lambda), so it needs no moc.
class SearchAnimation : public QWidget {
public:
  explicit SearchAnimation(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumSize(160, 160);
    m_timer = new QTimer(this);
    m_timer->setInterval(33); // ~30 fps
    QObject::connect(m_timer, &QTimer::timeout, this, [this] {
      m_phase += 0.05;
      update();
    });
  }

  void setActive(bool on) {
    if (on == m_timer->isActive())
      return;
    if (on)
      m_timer->start();
    else
      m_timer->stop();
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::black);

    const QPointF c = rect().center();
    const double maxR = 0.5 * std::min(width(), height()) - 4.0;
    if (maxR <= 0.0)
      return;

    // Three rings expanding outward, each fading as it grows, then recycling.
    constexpr int kRings = 3;
    for (int i = 0; i < kRings; ++i) {
      double t = std::fmod(m_phase + double(i) / kRings, 1.0);
      const double r = t * maxR;
      const int alpha = static_cast<int>(220 * (1.0 - t));
      QPen pen(QColor(255, 255, 255, alpha));
      pen.setWidth(2);
      p.setPen(pen);
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(c, r, r);
    }

    // Rotating sweep line with a faint trailing wedge, like a radar hand.
    const double angle = m_phase * 2.0 * M_PI;
    QPainterPath wedge;
    wedge.moveTo(c);
    wedge.arcTo(QRectF(c.x() - maxR, c.y() - maxR, 2 * maxR, 2 * maxR),
                -qRadiansToDegrees(angle), 28.0);
    wedge.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 40));
    p.drawPath(wedge);

    QPen hand(QColor(255, 255, 255, 220));
    hand.setWidth(2);
    p.setPen(hand);
    p.drawLine(c, c + QPointF(std::cos(angle) * maxR, std::sin(angle) * maxR));

    // A steady centre dot so the origin reads even between pulses.
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(c, 3.0, 3.0);
  }

private:
  QTimer *m_timer;
  double m_phase = 0.0;
};

// The "signal found" splash: a targeting reticle snaps shut around the detected
// centre frequency, a ring keeps breathing behind it, and a segmented meter
// fills to the peak level. Runs a repaint timer only while shown. Like
// SearchAnimation it declares no signals/slots, so it needs no moc.
class FoundDisplay : public QWidget {
public:
  explicit FoundDisplay(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumHeight(210);
    m_timer = new QTimer(this);
    m_timer->setInterval(33); // ~30 fps
    QObject::connect(m_timer, &QTimer::timeout, this, [this] {
      m_t += 0.033;
      if (m_lock < 1.0)
        m_lock = std::min(1.0, m_lock + 0.033 / kLockSeconds);
      update();
    });
  }

  // Latch a detection and replay the lock-on intro from the start.
  void setDetection(double frequencyHz, float powerDb) {
    m_freqHz = frequencyHz;
    m_powerDb = powerDb;
    m_t = 0.0;
    m_lock = 0.0;
    m_timer->start();
    update();
  }

  void stop() { m_timer->stop(); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::black);

    const QPointF c = rect().center();
    // Smoothstep the lock 0..1 so the reticle decelerates as it settles.
    const double e = m_lock * m_lock * (3.0 - 2.0 * m_lock);

    // Corner brackets fly in from beyond the frame and lock onto the text box.
    const double boxW = std::min(width() - 24, 250) * 0.5;
    const double boxH = 48.0;
    const double slide = (1.0 - e) * 64.0;
    const double bx = boxW + slide;
    const double by = boxH + slide;
    const double arm = 16.0;
    QPen bracket(QColor(255, 255, 255, int(70 + 185 * e)));
    bracket.setWidth(2);
    p.setPen(bracket);
    const QPointF corners[4] = {{c.x() - bx, c.y() - by},
                                {c.x() + bx, c.y() - by},
                                {c.x() + bx, c.y() + by},
                                {c.x() - bx, c.y() + by}};
    const int sx[4] = {1, -1, -1, 1};
    const int sy[4] = {1, 1, -1, -1};
    for (int i = 0; i < 4; ++i) {
      p.drawLine(corners[i], corners[i] + QPointF(sx[i] * arm, 0));
      p.drawLine(corners[i], corners[i] + QPointF(0, sy[i] * arm));
    }

    // Breathing ellipse behind the read-out, fading in with the lock.
    const double breathe = 0.5 + 0.5 * std::sin(m_t * 3.0);
    QPen ring(QColor(255, 255, 255, int((25 + 45 * breathe) * e)));
    ring.setWidth(2);
    p.setPen(ring);
    p.setBrush(Qt::NoBrush);
    const double rr = boxW + 12 + 4 * breathe;
    p.drawEllipse(c, rr, rr * 0.6);

    // Header tag above the box.
    QFont tag = font();
    tag.setBold(true);
    tag.setPixelSize(12);
    p.setFont(tag);
    p.setPen(QColor(255, 255, 255, int(255 * e)));
    p.drawText(QRectF(0, c.y() - by - 22, width(), 18),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QStringLiteral("◈ SIGNAL LOCKED ◈"));

    // The big centre-frequency read-out; shrinks to fit a wide value.
    const QString number = QString::number(m_freqHz / 1e6, 'f', 3);
    QFont big = font();
    big.setBold(true);
    big.setPixelSize(40);
    QFontMetrics fm(big);
    while (fm.horizontalAdvance(number) > width() - 30 &&
           big.pixelSize() > 16) {
      big.setPixelSize(big.pixelSize() - 1);
      fm = QFontMetrics(big);
    }
    p.setFont(big);
    p.setPen(QColor(255, 255, 255, int(255 * e)));
    p.drawText(QRectF(0, c.y() - boxH, width(), boxH), Qt::AlignCenter, number);

    QFont unit = font();
    unit.setPixelSize(14);
    p.setFont(unit);
    p.setPen(QColor(200, 200, 200, int(255 * e)));
    p.drawText(QRectF(0, c.y() + boxH - 22, width(), 18), Qt::AlignHCenter,
               QStringLiteral("MHz"));

    // Signal-strength meter: a segmented bar filled from the peak level, plus
    // the numeric dB. Grows in as the lock completes.
    constexpr double kLo = -60.0, kHi = 0.0;
    const double frac =
        std::clamp((double(m_powerDb) - kLo) / (kHi - kLo), 0.0, 1.0);
    constexpr int kSeg = 12;
    const int lit = int(std::lround(frac * kSeg * e));
    constexpr double segW = 12.0, segH = 8.0, segGap = 3.0;
    const double totalW = kSeg * segW + (kSeg - 1) * segGap;
    double x = c.x() - totalW * 0.5;
    const double y = c.y() + by + 8;
    for (int i = 0; i < kSeg; ++i) {
      const QRectF seg(x, y, segW, segH);
      if (i < lit) {
        p.fillRect(seg, Qt::white);
      } else {
        p.setPen(QColor(90, 90, 90));
        p.setBrush(Qt::NoBrush);
        p.drawRect(seg);
      }
      x += segW + segGap;
    }
    QFont lvl = font();
    lvl.setPixelSize(12);
    p.setFont(lvl);
    p.setPen(QColor(255, 255, 255, int(255 * e)));
    p.drawText(QRectF(0, y + segH + 3, width(), 16), Qt::AlignHCenter,
               QStringLiteral("%1 dB").arg(double(m_powerDb), 0, 'f', 1));
  }

private:
  static constexpr double kLockSeconds = 0.55;
  QTimer *m_timer;
  double m_t = 0.0;
  double m_lock = 0.0;
  double m_freqHz = 0.0;
  float m_powerDb = 0.0f;
};

IdentifierProgram::IdentifierProgram(QWidget *parent)
    : QWidget(parent), m_band(new QComboBox(this)),
      m_threshold(new QSpinBox(this)), m_pages(new QStackedWidget(this)),
      m_startButton(new QPushButton(tr("START"), this)),
      m_stopButton(new QPushButton(tr("STOP"), this)),
      m_resetButton(new QPushButton(tr("RESET"), this)),
      m_animation(new SearchAnimation(this)),
      m_found(new FoundDisplay(this)) {
  for (const Band &b : kBands)
    m_band->addItem(QString::fromUtf8(b.name));
  m_band->setCurrentIndex(1); // default to 433.92 MHz

  // Peak level: the trigger, in the same dB scale the detector reports. -20 dB
  // by default, as requested — the only numeric input in the program.
  m_threshold->setRange(-120, 20);
  m_threshold->setValue(-20);
  m_threshold->setSuffix(tr(" dB"));

  // --- Idle page: band + peak level over one big centred START button. ---
  auto *idle = new QWidget(this);
  auto *idleLayout = new QVBoxLayout(idle);
  idleLayout->setContentsMargins(16, 16, 16, 16);
  idleLayout->setSpacing(10);
  idleLayout->addStretch(1);

  auto *bandLabel = new QLabel(tr("BAND"), idle);
  bandLabel->setAlignment(Qt::AlignCenter);
  idleLayout->addWidget(bandLabel);
  idleLayout->addWidget(m_band);

  auto *levelLabel = new QLabel(tr("PEAK LEVEL"), idle);
  levelLabel->setAlignment(Qt::AlignCenter);
  idleLayout->addWidget(levelLabel);
  idleLayout->addWidget(m_threshold);

  idleLayout->addSpacing(12);
  // The one big action: a tall square-ish START in the centre.
  m_startButton->setMinimumHeight(88);
  QFont startFont = m_startButton->font();
  startFont.setPointSize(startFont.pointSize() + 12);
  startFont.setBold(true);
  m_startButton->setFont(startFont);
  m_startButton->setCursor(Qt::PointingHandCursor);
  idleLayout->addWidget(m_startButton);
  idleLayout->addStretch(2);

  // --- Searching page: the radar animation, a label, and STOP. ---
  auto *searching = new QWidget(this);
  auto *searchLayout = new QVBoxLayout(searching);
  searchLayout->setContentsMargins(16, 16, 16, 16);
  searchLayout->setSpacing(10);
  searchLayout->addWidget(m_animation, 1);
  auto *searchingLabel = new QLabel(tr("SEARCHING…"), searching);
  searchingLabel->setAlignment(Qt::AlignCenter);
  QFont searchFont = searchingLabel->font();
  searchFont.setPointSize(searchFont.pointSize() + 4);
  searchFont.setBold(true);
  searchingLabel->setFont(searchFont);
  searchLayout->addWidget(searchingLabel);
  m_stopButton->setMinimumHeight(44);
  m_stopButton->setCursor(Qt::PointingHandCursor);
  searchLayout->addWidget(m_stopButton);

  // --- Found page: the animated lock-on read-out, then RESET (rescan) + STOP.
  auto *found = new QWidget(this);
  auto *foundLayout = new QVBoxLayout(found);
  foundLayout->setContentsMargins(16, 16, 16, 16);
  foundLayout->setSpacing(8);
  foundLayout->addWidget(m_found, 1);

  auto *foundButtons = new QVBoxLayout;
  foundButtons->setSpacing(8);
  m_resetButton->setMinimumHeight(44);
  m_resetButton->setCursor(Qt::PointingHandCursor);
  auto *foundStop = new QPushButton(tr("STOP"), found);
  foundStop->setMinimumHeight(44);
  foundStop->setCursor(Qt::PointingHandCursor);
  foundButtons->addWidget(m_resetButton);
  foundButtons->addWidget(foundStop);
  foundLayout->addLayout(foundButtons);

  m_pages->addWidget(idle);      // Page::Idle
  m_pages->addWidget(searching); // Page::Searching
  m_pages->addWidget(found);     // Page::Found

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_pages);

  connect(m_startButton, &QPushButton::clicked, this,
          &IdentifierProgram::startRequested);
  connect(m_stopButton, &QPushButton::clicked, this,
          &IdentifierProgram::stopRequested);
  connect(foundStop, &QPushButton::clicked, this,
          &IdentifierProgram::stopRequested);
  connect(m_resetButton, &QPushButton::clicked, this,
          &IdentifierProgram::resetRequested);

  showPage(Page::Idle);
}

QString IdentifierProgram::infoText() {
  return tr(
      "<p>The <b>Identifier</b> finds the centre frequency of a signal in a "
      "common band. Pick a <b>band</b>, set the <b>peak level</b>, and press "
      "<b>START</b>. It listens on the receiver and, the moment a peak crosses "
      "the level, shows the signal's centre frequency in big text.</p>"
      "<p><b>How to use</b></p>"
      "<ol>"
      "<li>In <b>DEVICEES</b>, select a <b>receiver</b>.</li>"
      "<li><b>Band</b> — one of the common ISM bands to scan.</li>"
      "<li><b>Peak level</b> — the trigger, in the same dB scale as the "
      "detector. Signals stronger than this are reported; the default is "
      "−20 dB.</li>"
      "<li>Press <b>START</b>. A radar animation shows the scan is live.</li>"
      "<li>On a hit, the centre frequency appears. <b>RESET</b> scans the same "
      "band again; <b>STOP</b> ends the scan.</li>"
      "</ol>"
      "<p>There is no waterfall or FFT here — just the found frequency.</p>");
}

StreamParams IdentifierProgram::streamParams() const {
  const Band &b = kBands[m_band->currentIndex()];
  StreamParams p;
  p.frequencyHz = b.frequencyMHz * 1e6;
  p.sampleRateHz = b.sampleRateMSps * 1e6;
  p.gainDb = 30.0; // fixed RX gain: the program exposes no gain control
  return p;
}

float IdentifierProgram::thresholdDb() const {
  return static_cast<float>(m_threshold->value());
}

void IdentifierProgram::showPage(Page page) {
  m_pages->setCurrentIndex(static_cast<int>(page));
  m_animation->setActive(page == Page::Searching);
  // The found splash is (re)started by showDetection(); just make sure its
  // repaint timer is idle whenever that page is not showing.
  if (page != Page::Found)
    m_found->stop();
  m_band->setEnabled(page == Page::Idle);
  m_threshold->setEnabled(page == Page::Idle);
}

void IdentifierProgram::setRunning(bool running) {
  showPage(running ? Page::Searching : Page::Idle);
}

void IdentifierProgram::showDetection(double frequencyHz, float powerDb) {
  m_found->setDetection(frequencyHz, powerDb);
  showPage(Page::Found);
}

} // namespace duality
