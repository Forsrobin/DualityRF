#include "ui/DebugWorkspace.h"

#include "dsp/FftEngine.h"
#include "dsp/Measurements.h"
#include "dsp/SpectrumAccumulator.h"
#include "storage/IqFileReader.h"
#include "ui/SpectrumWidget.h"
#include "ui/WaterfallWidget.h"

#include <QComboBox>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace duality {

namespace {

constexpr int kFftSize = 4096;
// Cap analysis reads so opening a multi-gigabyte recording stays instant.
constexpr qint64 kMaxAnalysisSamples = 4 * 1024 * 1024;

QString formatHz(double hz)
{
    if (std::abs(hz) >= 1e6)
        return QStringLiteral("%1 MHz").arg(hz / 1e6, 0, 'f', 3);
    if (std::abs(hz) >= 1e3)
        return QStringLiteral("%1 kHz").arg(hz / 1e3, 0, 'f', 1);
    return QStringLiteral("%1 Hz").arg(hz, 0, 'f', 0);
}

// Renders an offline waterfall: one FFT row per evenly spaced slice of the
// recording, covering the whole file regardless of its length.
QVector<QVector<float>> waterfallRows(const QString &path,
                                      SampleFormat format, int rows)
{
    QVector<QVector<float>> out;
    IqFileReader reader;
    if (!reader.open(path, format))
        return out;

    const qint64 total = reader.totalSamples();
    if (total < kFftSize)
        return out;
    const qint64 stride = std::max<qint64>(kFftSize, total / rows);

    SpectrumAccumulator acc(kFftSize);
    std::vector<Complex> block(kFftSize);
    for (qint64 pos = 0; pos + kFftSize <= total; pos += stride) {
        reader.seekSample(pos);
        if (reader.read(block) < block.size())
            break;
        acc.reset();
        acc.process(block);
        out.push_back(acc.averageDb());
    }
    return out;
}

} // namespace

DebugWorkspace::DebugWorkspace(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_sessionCombo(new QComboBox(this))
    , m_recordingA(new QComboBox(this))
    , m_recordingB(new QComboBox(this))
    , m_replayA(new QPushButton(tr("REPLAY A"), this))
    , m_replayB(new QPushButton(tr("REPLAY B"), this))
    , m_spectrum(new SpectrumWidget(this))
    , m_waterfall(new WaterfallWidget(this))
    , m_table(new QTableWidget(this))
{
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Session"), this));
    top->addWidget(m_sessionCombo, 2);
    top->addWidget(new QLabel(tr("A"), this));
    top->addWidget(m_recordingA, 1);
    top->addWidget(new QLabel(tr("B"), this));
    top->addWidget(m_recordingB, 1);

    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Property"), tr("A"), tr("B")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *plots = new QSplitter(Qt::Vertical, this);
    plots->addWidget(m_spectrum);
    plots->addWidget(m_waterfall);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(plots);
    split->addWidget(m_table);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_replayA);
    buttons->addWidget(m_replayB);
    buttons->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(split, 1);
    layout->addLayout(buttons);

    connect(m_sessionCombo, &QComboBox::currentIndexChanged, this,
            &DebugWorkspace::onSessionChanged);
    connect(m_recordingA, &QComboBox::currentIndexChanged, this,
            &DebugWorkspace::onRecordingChanged);
    connect(m_recordingB, &QComboBox::currentIndexChanged, this,
            &DebugWorkspace::onRecordingChanged);
    connect(m_replayA, &QPushButton::clicked, this, [this] {
        const int idx = m_recordingA->currentIndex();
        if (m_session && idx >= 0 && idx < m_session->recordings().size()) {
            const RecordingMetadata &m = m_session->recordings()[idx];
            emit replayRequested(m_session->recordingPath(m), m);
        }
    });
    connect(m_replayB, &QPushButton::clicked, this, [this] {
        const int idx = m_recordingB->currentIndex();
        if (m_session && idx >= 0 && idx < m_session->recordings().size()) {
            const RecordingMetadata &m = m_session->recordings()[idx];
            emit replayRequested(m_session->recordingPath(m), m);
        }
    });

    refreshSessions();
}

void DebugWorkspace::refreshSessions()
{
    const QString previous = m_sessionCombo->currentText();
    m_sessionCombo->blockSignals(true);
    m_sessionCombo->clear();
    for (const QString &dir : m_store->listSessionDirs())
        m_sessionCombo->addItem(QDir(dir).dirName(), dir);
    m_sessionCombo->blockSignals(false);

    const int idx = m_sessionCombo->findText(previous);
    m_sessionCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    onSessionChanged(m_sessionCombo->currentIndex());
}

void DebugWorkspace::onSessionChanged(int index)
{
    m_session.reset();
    m_recordingA->clear();
    m_recordingB->clear();
    if (index < 0) {
        updateViews();
        return;
    }
    m_session = Session::load(m_sessionCombo->itemData(index).toString());
    if (!m_session) {
        updateViews();
        return;
    }
    for (const RecordingMetadata &m : m_session->recordings()) {
        m_recordingA->addItem(m.file);
        m_recordingB->addItem(m.file);
    }
    if (m_recordingB->count() > 1)
        m_recordingB->setCurrentIndex(1);
    updateViews();
}

void DebugWorkspace::onRecordingChanged()
{
    updateViews();
}

DebugWorkspace::Analysis DebugWorkspace::analyzeRecording(int recordingIndex)
{
    Analysis a;
    if (!m_session || recordingIndex < 0 ||
        recordingIndex >= m_session->recordings().size())
        return a;
    a.meta = m_session->recordings()[recordingIndex];

    // Prefer the session's fft.cache; fall back to (and refill it from) the
    // IQ file itself.
    QVector<double> linear;
    if (auto cached = m_session->fftCache().lookup(a.meta.file)) {
        a.spectrumDb = *cached;
        linear.resize(a.spectrumDb.size());
        for (int i = 0; i < a.spectrumDb.size(); ++i)
            linear[i] = std::pow(10.0, a.spectrumDb[i] / 10.0);
    } else {
        SpectrumAccumulator acc(kFftSize);
        IqFileReader reader;
        if (!reader.open(m_session->recordingPath(a.meta), a.meta.format))
            return a;
        std::vector<Complex> chunk(65536);
        qint64 remaining = std::min(reader.totalSamples(), kMaxAnalysisSamples);
        while (remaining > 0) {
            const std::size_t got = reader.read(std::span(
                chunk.data(),
                std::min<qint64>(remaining, static_cast<qint64>(chunk.size()))));
            if (got == 0)
                break;
            acc.process(std::span(chunk.data(), got));
            remaining -= static_cast<qint64>(got);
        }
        if (acc.blocks() == 0)
            return a;
        a.spectrumDb = acc.averageDb();
        linear = acc.linearAverage();
        m_session->fftCache().append(a.meta.file, a.spectrumDb);
    }

    const SpectrumStats stats = analyzeSpectrum(linear, a.meta.sampleRateHz);
    a.occupiedBandwidthHz = stats.occupiedBandwidthHz;
    a.meanDb = stats.meanDb;
    a.peakDb = stats.peakDb;
    a.peakOffsetHz = stats.peakOffsetHz;
    a.valid = true;
    return a;
}

void DebugWorkspace::updateViews()
{
    const Analysis a = analyzeRecording(m_recordingA->currentIndex());
    const Analysis b = analyzeRecording(m_recordingB->currentIndex());

    m_spectrum->clear();
    if (a.valid) {
        m_spectrum->setAxis(a.meta.frequencyHz, a.meta.sampleRateHz);
        m_spectrum->setTrace(a.spectrumDb);
        m_waterfall->setRows(waterfallRows(m_session->recordingPath(a.meta),
                                           a.meta.format, 256));
    } else {
        m_waterfall->clear();
    }
    if (b.valid)
        m_spectrum->setSecondaryTrace(b.spectrumDb);

    m_replayA->setEnabled(a.valid);
    m_replayB->setEnabled(b.valid);
    fillTable(a, b);
}

void DebugWorkspace::fillTable(const Analysis &a, const Analysis &b)
{
    struct Row {
        QString name;
        QString valueA;
        QString valueB;
    };
    const auto describe = [](const Analysis &x) -> QStringList {
        if (!x.valid)
            return QStringList{"", "", "", "", "", "", "", "", "", "", ""};
        return QStringList{
            formatHz(x.meta.frequencyHz),
            QStringLiteral("%1 MS/s").arg(x.meta.sampleRateHz / 1e6, 0, 'f', 3),
            x.meta.bandwidthHz > 0 ? formatHz(x.meta.bandwidthHz)
                                   : QStringLiteral("auto"),
            QStringLiteral("%1 s").arg(x.meta.durationSec, 0, 'f', 2),
            QString::number(x.meta.samples),
            x.meta.startedUtc.toString(Qt::ISODate),
            QStringLiteral("%1 (%2)").arg(x.meta.deviceLabel,
                                          x.meta.deviceSerial),
            formatHz(x.occupiedBandwidthHz),
            QStringLiteral("%1 dBFS").arg(x.meanDb, 0, 'f', 1),
            QStringLiteral("%1 dBFS").arg(x.peakDb, 0, 'f', 1),
            formatHz(x.peakOffsetHz),
        };
    };
    const QStringList names{
        tr("Frequency"),       tr("Sample rate"),   tr("Bandwidth"),
        tr("Duration"),        tr("Samples"),       tr("Started (UTC)"),
        tr("Device"),          tr("Occupied BW (99%)"), tr("Mean power"),
        tr("Peak power"),      tr("Peak offset"),
    };
    const QStringList va = describe(a);
    const QStringList vb = describe(b);

    m_table->setRowCount(names.size());
    for (int i = 0; i < names.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(names[i]));
        m_table->setItem(i, 1, new QTableWidgetItem(va[i]));
        m_table->setItem(i, 2, new QTableWidgetItem(vb[i]));
    }
    m_table->resizeColumnsToContents();
}

} // namespace duality
