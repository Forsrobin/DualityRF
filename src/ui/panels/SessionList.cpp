#include "ui/panels/SessionList.h"

#include "storage/Session.h"

#include <QColor>
#include <QFont>
#include <QListWidget>
#include <QSize>
#include <QVBoxLayout>

namespace duality {

namespace {
constexpr int kDirRole = Qt::UserRole;
} // namespace

SessionList::SessionList(SessionStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_list(new QListWidget(this))
{
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setUniformItemSizes(false);
    m_list->setWordWrap(true);
    m_list->setCursor(Qt::PointingHandCursor);
    // Alternating rows (a slightly brighter black) make each session easier to
    // separate at a glance.
    m_list->setAlternatingRowColors(true);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background:#000000; alternate-background-color:#151515; }"
        "QListWidget::item { padding:4px 6px; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);

    // A tap opens the management program on that session.
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
                const QString dir = item->data(kDirRole).toString();
                if (!dir.isEmpty())
                    emit sessionActivated(dir);
            });

    refresh();
}

void SessionList::refresh()
{
    m_list->clear();

    const QStringList dirs = m_store->listSessionDirs(); // newest first
    for (int i = 0; i < dirs.size(); ++i) {
        auto session = Session::load(dirs[i]);
        if (!session)
            continue;

        const int recordings = session->recordings().size();
        const QString created =
            session->created().toLocalTime().toString(
                QStringLiteral("yyyy-MM-dd HH:mm"));
        QString title = session->name();
        const bool newest = (i == 0);
        if (newest)
            title += QStringLiteral("    ★ NEWEST");
        const QString countText = QString::number(recordings) +
                                  (recordings == 1 ? tr(" recording")
                                                   : tr(" recordings"));
        const QString subtitle =
            countText + QStringLiteral("  ·  ") + created;

        auto *item = new QListWidgetItem(title + QStringLiteral("\n") + subtitle,
                                         m_list);
        item->setData(kDirRole, session->dir());
        item->setToolTip(tr("Open in Sessions"));
        // Reserve two text lines so the count/date subtitle is never clipped.
        const int lineH = m_list->fontMetrics().height();
        item->setSizeHint(QSize(0, lineH * 2 + 16));
        if (newest) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setForeground(QColor(QStringLiteral("#7CFC98")));
        }
    }

    if (m_list->count() == 0) {
        auto *empty = new QListWidgetItem(tr("No sessions yet"), m_list);
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(QColor(QStringLiteral("#888888")));
    }
}

} // namespace duality
