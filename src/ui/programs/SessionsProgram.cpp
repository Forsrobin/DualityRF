#include "ui/programs/SessionsProgram.h"

#include "storage/Session.h"
#include "ui/panels/PlaybackPanel.h"

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace duality {

namespace {

constexpr int kDirRole = Qt::UserRole;
constexpr int kRecRole = Qt::UserRole;

// Alternating rows (a slightly brighter black) to separate list entries.
const auto kListStyle = QStringLiteral(
    "QListWidget { background:#000000; alternate-background-color:#151515; }"
    "QListWidget::item { padding:4px 6px; }");

// "N recording" / "N recordings", built without chained arg() so the count is
// always right.
QString recordingCountText(int n)
{
    return QString::number(n) +
           (n == 1 ? QObject::tr(" recording") : QObject::tr(" recordings"));
}

// A bold, underlined section heading matching the dividers used elsewhere.
QLabel *makeSection(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(
        QStringLiteral("color:#aaaaaa; font-weight:bold; margin-top:6px; "
                       "padding-bottom:2px; border-bottom:1px solid #555555;"));
    return label;
}

QString formatBandwidth(double hz)
{
    if (hz <= 0.0)
        return QStringLiteral("auto");
    return QStringLiteral("%1 MHz").arg(hz / 1e6, 0, 'f', 3);
}

// One recording's metadata as a compact key/value block.
QString formatInfo(const RecordingMetadata &m)
{
    const auto row = [](const QString &k, const QString &v) {
        return QStringLiteral(
                   "<tr><td style='color:#888;padding-right:10px;'>%1</td>"
                   "<td>%2</td></tr>")
            .arg(k, v);
    };
    QString device = m.deviceLabel.isEmpty() ? m.deviceDriver : m.deviceLabel;
    if (device.isEmpty())
        device = QStringLiteral("—");
    QString rows;
    rows += row(QObject::tr("Started"),
                m.startedUtc.toLocalTime().toString(
                    QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    rows += row(QObject::tr("Duration"),
                QStringLiteral("%1 s").arg(m.durationSec, 0, 'f', 2));
    rows += row(QObject::tr("Samples"), QString::number(m.samples));
    rows += row(QObject::tr("Frequency"),
                QStringLiteral("%1 MHz").arg(m.frequencyHz / 1e6, 0, 'f', 3));
    rows += row(QObject::tr("Sample rate"),
                QStringLiteral("%1 MS/s").arg(m.sampleRateHz / 1e6, 0, 'f', 3));
    rows += row(QObject::tr("Bandwidth"), formatBandwidth(m.bandwidthHz));
    rows += row(QObject::tr("RX gain"),
                QStringLiteral("%1 dB").arg(m.rxGainDb, 0, 'f', 1));
    rows += row(QObject::tr("Trigger"), m.trigger);
    rows += row(QObject::tr("Device"), device);
    rows += row(QObject::tr("Concurrent TX"),
                m.concurrentTx.isEmpty() ? QObject::tr("none") : m.concurrentTx);
    rows += row(QObject::tr("Format"),
                QString::fromLatin1(sampleFormatName(m.format)));
    return QStringLiteral("<table>%1</table>").arg(rows);
}

} // namespace

SessionsProgram::SessionsProgram(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_stack(new QStackedWidget(this))
    , m_sessions(new QListWidget(this))
    , m_detailTitle(new QLabel(this))
    , m_detailSubtitle(new QLabel(this))
    , m_recordings(new QListWidget(this))
    , m_recordingBox(new QWidget(this))
    , m_info(new QLabel(this))
    , m_deleteRecordingButton(new QPushButton(tr("DELETE RECORDING"), this))
    , m_player(new PlaybackPanel(store, this))
{
    // ---- List page: NEW SESSION + the flat session list. ----
    auto *listPage = new QWidget(this);
    auto *listLayout = new QVBoxLayout(listPage);
    listLayout->setContentsMargins(6, 6, 6, 6);
    auto *listHeader = new QHBoxLayout;
    auto *newButton = new QPushButton(tr("NEW SESSION"), listPage);
    newButton->setCursor(Qt::PointingHandCursor);
    connect(newButton, &QPushButton::clicked, this,
            &SessionsProgram::newSessionRequested);
    auto *deleteAll = new QPushButton(tr("DELETE ALL"), listPage);
    deleteAll->setCursor(Qt::PointingHandCursor);
    deleteAll->setStyleSheet(
        QStringLiteral("QPushButton { background:#cc0000; color:#ffffff; "
                       "border:1px solid #990000; font-weight:bold; }"
                       "QPushButton:pressed { background:#990000; }"));
    connect(deleteAll, &QPushButton::clicked, this,
            &SessionsProgram::promptDeleteAllSessions);
    listHeader->addWidget(newButton, 1);
    listHeader->addWidget(deleteAll);
    m_sessions->setSelectionMode(QAbstractItemView::NoSelection);
    m_sessions->setWordWrap(true);
    m_sessions->setCursor(Qt::PointingHandCursor);
    m_sessions->setAlternatingRowColors(true);
    m_sessions->setStyleSheet(kListStyle);
    connect(m_sessions, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
                const QString dir = item->data(kDirRole).toString();
                if (!dir.isEmpty())
                    openSession(dir);
            });
    listLayout->addLayout(listHeader);
    listLayout->addWidget(m_sessions, 1);

    // ---- Detail page (scrollable): header, recordings, info, player. ----
    auto *detailBody = new QWidget;
    auto *detailLayout = new QVBoxLayout(detailBody);
    detailLayout->setContentsMargins(6, 6, 6, 6);

    auto *headerRow = new QHBoxLayout;
    auto *back = new QPushButton(tr("‹ Sessions"), detailBody);
    back->setCursor(Qt::PointingHandCursor);
    connect(back, &QPushButton::clicked, this, &SessionsProgram::showList);
    auto *deleteSession = new QPushButton(tr("DELETE"), detailBody);
    deleteSession->setCursor(Qt::PointingHandCursor);
    deleteSession->setStyleSheet(
        QStringLiteral("QPushButton { background:#cc0000; color:#ffffff; "
                       "border:1px solid #990000; font-weight:bold; }"
                       "QPushButton:pressed { background:#990000; }"));
    connect(deleteSession, &QPushButton::clicked, this,
            &SessionsProgram::promptDeleteSession);
    headerRow->addWidget(back);
    headerRow->addStretch(1);
    headerRow->addWidget(deleteSession);
    detailLayout->addLayout(headerRow);

    QFont titleFont = m_detailTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
    m_detailTitle->setFont(titleFont);
    m_detailTitle->setWordWrap(true);
    m_detailSubtitle->setStyleSheet(QStringLiteral("color:#888888;"));
    detailLayout->addWidget(m_detailTitle);
    detailLayout->addWidget(m_detailSubtitle);

    detailLayout->addWidget(makeSection(tr("Recordings"), detailBody));
    m_recordings->setWordWrap(true);
    m_recordings->setCursor(Qt::PointingHandCursor);
    m_recordings->setMaximumHeight(140);
    m_recordings->setAlternatingRowColors(true);
    m_recordings->setStyleSheet(kListStyle);
    connect(m_recordings, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
                showRecording(item->data(kRecRole).toInt());
            });
    detailLayout->addWidget(m_recordings);

    // The player + info + delete group, hidden until a recording is picked.
    // Playback sits above the read-only info so the controls are reachable
    // without scrolling past the metadata.
    auto *recBoxLayout = new QVBoxLayout(m_recordingBox);
    recBoxLayout->setContentsMargins(0, 0, 0, 0);
    recBoxLayout->addWidget(makeSection(tr("Playback"), m_recordingBox));
    recBoxLayout->addWidget(m_player);
    recBoxLayout->addWidget(makeSection(tr("Recording info"), m_recordingBox));
    m_info->setTextFormat(Qt::RichText);
    m_info->setWordWrap(true);
    recBoxLayout->addWidget(m_info);
    m_deleteRecordingButton->setCursor(Qt::PointingHandCursor);
    connect(m_deleteRecordingButton, &QPushButton::clicked, this,
            [this] { promptDeleteRecording(m_currentRecording); });
    recBoxLayout->addWidget(m_deleteRecordingButton);
    m_recordingBox->setVisible(false);
    detailLayout->addWidget(m_recordingBox);
    detailLayout->addStretch(1);

    auto *detailScroll = new QScrollArea(this);
    detailScroll->setWidget(detailBody);
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_stack->addWidget(listPage);     // index 0
    m_stack->addWidget(detailScroll); // index 1

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);

    rebuildSessionList();
}

void SessionsProgram::refresh()
{
    rebuildSessionList();
    // If a detail page is open, reload that session from disk (its recordings
    // may have changed) or fall back to the list if it is gone.
    if (m_stack->currentIndex() == 1 && m_session) {
        auto reloaded = Session::load(m_session->dir());
        if (reloaded) {
            m_session = std::move(reloaded);
            rebuildDetail();
        } else {
            showList();
        }
    }
    m_player->refresh();
}

void SessionsProgram::showList()
{
    rebuildSessionList();
    m_stack->setCurrentIndex(0);
}

void SessionsProgram::openSession(const QString &sessionDir)
{
    m_session = Session::load(sessionDir);
    if (!m_session) {
        showList();
        return;
    }
    rebuildDetail();
    m_stack->setCurrentIndex(1);
}

void SessionsProgram::rebuildSessionList()
{
    m_sessions->clear();
    const QStringList dirs = m_store->listSessionDirs(); // newest first
    for (int i = 0; i < dirs.size(); ++i) {
        auto session = Session::load(dirs[i]);
        if (!session)
            continue;
        const int recordings = session->recordings().size();
        const QString created = session->created().toLocalTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm"));
        const bool newest = (i == 0);
        QString title = session->name();
        if (newest)
            title += QStringLiteral("    ★ NEWEST");
        const QString subtitle =
            recordingCountText(recordings) + QStringLiteral("  ·  ") + created;
        auto *item = new QListWidgetItem(
            title + QStringLiteral("\n") + subtitle, m_sessions);
        item->setData(kDirRole, session->dir());
        const int lineH = m_sessions->fontMetrics().height();
        item->setSizeHint(QSize(0, lineH * 2 + 16));
        if (newest) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setForeground(QColor(QStringLiteral("#7CFC98")));
        }
    }
    if (m_sessions->count() == 0) {
        auto *empty = new QListWidgetItem(tr("No sessions yet"), m_sessions);
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(QColor(QStringLiteral("#888888")));
    }
}

void SessionsProgram::rebuildDetail()
{
    if (!m_session)
        return;
    m_detailTitle->setText(m_session->name());
    const auto &recordings = m_session->recordings();
    m_detailSubtitle->setText(
        tr("Created %1  ·  %2")
            .arg(m_session->created().toLocalTime().toString(
                QStringLiteral("yyyy-MM-dd HH:mm")))
            .arg(recordingCountText(static_cast<int>(recordings.size()))));

    m_recordings->clear();
    for (int i = 0; i < recordings.size(); ++i) {
        const RecordingMetadata &m = recordings[i];
        const QString text =
            QStringLiteral("%1\n%2 MHz  ·  %3 MS/s  ·  %4 s")
                .arg(m.file)
                .arg(m.frequencyHz / 1e6, 0, 'f', 3)
                .arg(m.sampleRateHz / 1e6, 0, 'f', 3)
                .arg(m.durationSec, 0, 'f', 1);
        auto *item = new QListWidgetItem(text, m_recordings);
        item->setData(kRecRole, i);
        const int lineH = m_recordings->fontMetrics().height();
        item->setSizeHint(QSize(0, lineH * 2 + 16));
    }
    if (recordings.isEmpty()) {
        auto *empty =
            new QListWidgetItem(tr("No recordings in this session"), m_recordings);
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(QColor(QStringLiteral("#888888")));
    }

    // Keep the current recording selected if it still exists, else collapse the
    // info/player box.
    if (m_currentRecording >= 0 && m_currentRecording < recordings.size())
        showRecording(m_currentRecording);
    else {
        m_currentRecording = -1;
        m_recordingBox->setVisible(false);
    }
}

void SessionsProgram::showRecording(int index)
{
    if (!m_session || index < 0 || index >= m_session->recordings().size()) {
        m_currentRecording = -1;
        m_recordingBox->setVisible(false);
        return;
    }
    m_currentRecording = index;
    const RecordingMetadata &meta = m_session->recordings()[index];
    m_info->setText(formatInfo(meta));
    m_player->select(m_session->dir(), meta.file);
    m_recordingBox->setVisible(true);
}

void SessionsProgram::promptDeleteSession()
{
    if (!m_session)
        return;
    const auto reply = QMessageBox::question(
        this, tr("Delete session"),
        tr("Permanently delete session %1 and all its recordings?")
            .arg(m_session->name()));
    if (reply != QMessageBox::Yes)
        return;
    m_store->deleteSession(m_session->dir());
    m_session.reset();
    m_currentRecording = -1;
    showList();
    emit sessionsChanged();
}

void SessionsProgram::promptDeleteAllSessions()
{
    const QStringList dirs = m_store->listSessionDirs();
    if (dirs.isEmpty()) {
        QMessageBox::information(this, tr("Delete all sessions"),
                                 tr("There are no sessions to delete."));
        return;
    }
    const auto reply = QMessageBox::question(
        this, tr("Delete all sessions"),
        tr("Permanently delete all %1 sessions and every recording they "
           "contain? This cannot be undone.")
            .arg(dirs.size()));
    if (reply != QMessageBox::Yes)
        return;
    for (const QString &dir : dirs)
        m_store->deleteSession(dir);
    m_session.reset();
    m_currentRecording = -1;
    showList();
    emit sessionsChanged();
}

void SessionsProgram::promptDeleteRecording(int index)
{
    if (!m_session || index < 0 || index >= m_session->recordings().size())
        return;
    const QString file = m_session->recordings()[index].file;
    const auto reply =
        QMessageBox::question(this, tr("Delete recording"),
                              tr("Permanently delete %1?").arg(file));
    if (reply != QMessageBox::Yes)
        return;
    m_session->removeRecording(index);
    // The list shifts after a delete; drop the selection rather than guess.
    m_currentRecording = -1;
    rebuildDetail();
    emit sessionsChanged();
}

} // namespace duality
