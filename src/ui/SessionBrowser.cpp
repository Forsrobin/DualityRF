#include "ui/SessionBrowser.h"

#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace duality {

namespace {
constexpr int kSessionRole = Qt::UserRole;
constexpr int kRecordingRole = Qt::UserRole + 1;
}

SessionBrowser::SessionBrowser(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_tree(new QTreeWidget(this))
    , m_newSessionButton(new QPushButton(tr("NEW SESSION"), this))
{
    m_tree->setHeaderLabels(
        {tr("Recording"), tr("Frequency"), tr("Rate"), tr("Duration")});
    m_tree->setRootIsDecorated(true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_newSessionButton);

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
            &SessionBrowser::onSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
            &SessionBrowser::onContextMenu);
    connect(m_newSessionButton, &QPushButton::clicked, this,
            &SessionBrowser::newSessionRequested);

    refresh();
}

void SessionBrowser::refresh()
{
    m_tree->clear();
    m_sessions.clear();

    for (const QString &dir : m_store->listSessionDirs()) {
        auto session = Session::load(dir);
        if (!session)
            continue;
        auto *sessionItem = new QTreeWidgetItem(m_tree);
        sessionItem->setText(0, session->name());
        sessionItem->setData(0, kSessionRole,
                             static_cast<int>(m_sessions.size()));
        sessionItem->setData(0, kRecordingRole, -1);

        const auto &recordings = session->recordings();
        for (int i = 0; i < recordings.size(); ++i) {
            const RecordingMetadata &m = recordings[i];
            auto *item = new QTreeWidgetItem(sessionItem);
            item->setText(0, m.file);
            item->setText(1, QStringLiteral("%1 MHz")
                                 .arg(m.frequencyHz / 1e6, 0, 'f', 3));
            item->setText(2, QStringLiteral("%1 MS/s")
                                 .arg(m.sampleRateHz / 1e6, 0, 'f', 3));
            item->setText(3,
                          QStringLiteral("%1 s").arg(m.durationSec, 0, 'f', 1));
            item->setData(0, kSessionRole,
                          static_cast<int>(m_sessions.size()));
            item->setData(0, kRecordingRole, i);
        }
        sessionItem->setExpanded(m_sessions.empty()); // newest session open
        m_sessions.push_back(std::move(session));
    }
    m_tree->resizeColumnToContents(0);
}

void SessionBrowser::onSelectionChanged()
{
    const auto items = m_tree->selectedItems();
    if (items.isEmpty())
        return;
    const int sessionIdx = items.first()->data(0, kSessionRole).toInt();
    const int recIdx = items.first()->data(0, kRecordingRole).toInt();
    if (recIdx < 0 || sessionIdx < 0 ||
        sessionIdx >= static_cast<int>(m_sessions.size()))
        return;
    const Session &session = *m_sessions[sessionIdx];
    emit recordingSelected(session.dir(), session.recordings()[recIdx]);
}

void SessionBrowser::onContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item)
        return;
    const int sessionIdx = item->data(0, kSessionRole).toInt();
    const int recIdx = item->data(0, kRecordingRole).toInt();
    if (sessionIdx < 0 || sessionIdx >= static_cast<int>(m_sessions.size()))
        return;
    Session &session = *m_sessions[sessionIdx];

    QMenu menu(this);
    if (recIdx >= 0) {
        // On a recording row: delete just that recording.
        const QString file = item->text(0);
        QAction *act = menu.addAction(tr("Delete recording \"%1\"").arg(file));
        if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == act) {
            const auto reply = QMessageBox::question(
                this, tr("Delete recording"),
                tr("Permanently delete %1?").arg(file));
            if (reply == QMessageBox::Yes) {
                session.removeRecording(recIdx);
                refresh();
            }
        }
        return;
    }

    // On a session row: delete the whole session directory.
    const QString dir = session.dir();
    const QString name = session.name();
    QAction *act = menu.addAction(tr("Delete session \"%1\"").arg(name));
    if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == act) {
        const auto reply = QMessageBox::question(
            this, tr("Delete session"),
            tr("Permanently delete session %1 and all its recordings?")
                .arg(name));
        if (reply == QMessageBox::Yes) {
            m_store->deleteSession(dir);
            refresh();
        }
    }
}

} // namespace duality
