#include "ui/panels/SessionBrowser.h"

#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

namespace duality {

namespace {
constexpr int kSessionRole = Qt::UserRole;
constexpr int kRecordingRole = Qt::UserRole + 1;
constexpr int kDeleteColumn = 4;
constexpr int kDeleteColumnWidth = 48;
}

SessionBrowser::SessionBrowser(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_tree(new QTreeWidget(this))
    , m_newSessionButton(new QPushButton(tr("NEW SESSION"), this))
{
    m_tree->setHeaderLabels({tr("Recording"), tr("Frequency"), tr("Rate"),
                             tr("Duration"), QString()});
    m_tree->setRootIsDecorated(true);
    // Pin the delete buttons to the far right: the name column absorbs the
    // spare width instead of the last column.
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(kDeleteColumn, QHeaderView::Fixed);
    m_tree->header()->resizeSection(kDeleteColumn, kDeleteColumnWidth);

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

    // Touch-friendly alternative to the context menu: one delete button per
    // row, in the fixed far-right column.
    const auto addDeleteButton = [this](QTreeWidgetItem *item,
                                        const std::function<void()> &onClick) {
        auto *button = new QPushButton(QStringLiteral("✕"), m_tree);
        button->setStyleSheet(QStringLiteral("padding: 10px 0px;"));
        button->setToolTip(tr("Delete"));
        connect(button, &QPushButton::clicked, this, onClick);
        m_tree->setItemWidget(item, kDeleteColumn, button);
    };

    for (const QString &dir : m_store->listSessionDirs()) {
        auto session = Session::load(dir);
        if (!session)
            continue;
        const int sessionIdx = static_cast<int>(m_sessions.size());
        auto *sessionItem = new QTreeWidgetItem(m_tree);
        sessionItem->setText(0, session->name());
        sessionItem->setData(0, kSessionRole, sessionIdx);
        sessionItem->setData(0, kRecordingRole, -1);
        addDeleteButton(sessionItem, [this, sessionIdx] {
            promptDeleteSession(sessionIdx);
        });

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
            item->setData(0, kSessionRole, sessionIdx);
            item->setData(0, kRecordingRole, i);
            addDeleteButton(item, [this, sessionIdx, i] {
                promptDeleteRecording(sessionIdx, i);
            });
        }
        sessionItem->setExpanded(m_sessions.empty()); // newest session open
        m_sessions.push_back(std::move(session));
    }
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
        QAction *act =
            menu.addAction(tr("Delete recording \"%1\"").arg(item->text(0)));
        if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == act)
            promptDeleteRecording(sessionIdx, recIdx);
        return;
    }

    // On a session row: delete the whole session directory.
    QAction *act =
        menu.addAction(tr("Delete session \"%1\"").arg(session.name()));
    if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == act)
        promptDeleteSession(sessionIdx);
}

void SessionBrowser::promptDeleteRecording(int sessionIdx, int recIdx)
{
    if (sessionIdx < 0 || sessionIdx >= static_cast<int>(m_sessions.size()))
        return;
    Session &session = *m_sessions[sessionIdx];
    if (recIdx < 0 || recIdx >= session.recordings().size())
        return;
    const QString file = session.recordings()[recIdx].file;
    const auto reply =
        QMessageBox::question(this, tr("Delete recording"),
                              tr("Permanently delete %1?").arg(file));
    if (reply == QMessageBox::Yes) {
        session.removeRecording(recIdx);
        refresh();
    }
}

void SessionBrowser::promptDeleteSession(int sessionIdx)
{
    if (sessionIdx < 0 || sessionIdx >= static_cast<int>(m_sessions.size()))
        return;
    const Session &session = *m_sessions[sessionIdx];
    const auto reply = QMessageBox::question(
        this, tr("Delete session"),
        tr("Permanently delete session %1 and all its recordings?")
            .arg(session.name()));
    if (reply == QMessageBox::Yes) {
        m_store->deleteSession(session.dir());
        refresh();
    }
}

} // namespace duality
