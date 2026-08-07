#include "ui/components/NumpadOverlay.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QBoxLayout>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFocusEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace duality {

namespace {

// Height of the pad as a slice of the (fixed-size) host window.
constexpr double kHeightFraction = 0.45;

// Plain numeric text (no prefix/suffix) for a spin box's current value, with
// trailing zeros trimmed so the buffer starts clean and editable.
QString plainValue(QAbstractSpinBox *box)
{
    if (auto *d = qobject_cast<QDoubleSpinBox *>(box)) {
        QString s = QString::number(d->value(), 'f', d->decimals());
        if (s.contains(QLatin1Char('.'))) {
            while (s.endsWith(QLatin1Char('0')))
                s.chop(1);
            if (s.endsWith(QLatin1Char('.')))
                s.chop(1);
        }
        return s;
    }
    if (auto *i = qobject_cast<QSpinBox *>(box))
        return QString::number(i->value());
    return QString();
}

bool allowsDecimals(QAbstractSpinBox *box)
{
    auto *d = qobject_cast<QDoubleSpinBox *>(box);
    return d && d->decimals() > 0;
}

bool allowsNegative(QAbstractSpinBox *box)
{
    if (auto *d = qobject_cast<QDoubleSpinBox *>(box))
        return d->minimum() < 0.0;
    if (auto *i = qobject_cast<QSpinBox *>(box))
        return i->minimum() < 0;
    return false;
}

} // namespace

NumpadOverlay::NumpadOverlay(QWidget *host)
    : QWidget(host)
    , m_host(host)
    , m_currentLabel(new QLabel(this))
    , m_newLabel(new QLabel(this))
    , m_dotKey(nullptr)
    , m_commaKey(nullptr)
    , m_signKey(nullptr)
{
    setObjectName(QStringLiteral("NumpadOverlay"));
    // A top border lifts the pad off the content it covers; the rest inherits
    // the app's monochrome theme.
    setStyleSheet(QStringLiteral(
        "#NumpadOverlay { background:#000000; border-top:1px solid #ffffff; }"));
    // The pad and its keys never take focus, so tapping them keeps the edited
    // field focused (and thus the pad open).
    setFocusPolicy(Qt::NoFocus);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar: original value on the left, live edit on the right. A framed
    // strip sits flush against the pad's top edge and is divided from the keys
    // below by a border, so the value readout reads as part of the pad.
    auto *headerBar = new QWidget(this);
    headerBar->setObjectName(QStringLiteral("NumpadHeader"));
    // A plain QWidget only paints a stylesheet background with this attribute,
    // otherwise just the border would show over a transparent bar.
    headerBar->setAttribute(Qt::WA_StyledBackground, true);
    // Blue readout bar with white text, framed to sit flush against the pad top.
    headerBar->setStyleSheet(QStringLiteral(
        "#NumpadHeader { background:#1e5fbf; border:1px solid #ffffff; }"));
    auto *header = new QHBoxLayout(headerBar);
    header->setContentsMargins(8, 6, 8, 6);
    m_currentLabel->setStyleSheet(
        QStringLiteral("color:#dce6f7; background:transparent;"));
    m_newLabel->setStyleSheet(
        QStringLiteral("color:#ffffff; background:transparent; font-weight:bold;"));
    m_newLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->addWidget(m_currentLabel, 1);
    header->addWidget(m_newLabel, 1);
    root->addWidget(headerBar);

    // Key grid: a phone-style numpad plus edit/confirm keys down the right.
    auto *keys = new QWidget(this);
    auto *grid = new QGridLayout(keys);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(3);
    const char *digits[4][3] = {
        {"7", "8", "9"}, {"4", "5", "6"}, {"1", "2", "3"}, {".", "0", ","}};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 3; ++c) {
            const QString label = QString::fromLatin1(digits[r][c]);
            QPushButton *key = makeKey(label);
            grid->addWidget(key, r, c);
            if (label == QLatin1String("."))
                m_dotKey = key;
            else if (label == QLatin1String(","))
                m_commaKey = key;
            connect(key, &QPushButton::clicked, this,
                    [this, label] { appendText(label); });
        }

    QPushButton *backKey = makeKey(QStringLiteral("⌫")); // ⌫
    QPushButton *clearKey = makeKey(QStringLiteral("C"));
    m_signKey = makeKey(QStringLiteral("±")); // ±
    QPushButton *doneKey = makeKey(QStringLiteral("✓")); // ✓
    connect(backKey, &QPushButton::clicked, this, &NumpadOverlay::backspace);
    connect(clearKey, &QPushButton::clicked, this, &NumpadOverlay::clearInput);
    connect(m_signKey, &QPushButton::clicked, this, &NumpadOverlay::toggleSign);
    connect(doneKey, &QPushButton::clicked, this, &NumpadOverlay::detach);
    grid->addWidget(backKey, 0, 3);
    grid->addWidget(clearKey, 1, 3);
    grid->addWidget(m_signKey, 2, 3);
    grid->addWidget(doneKey, 3, 3);

    // Even, expanding cells so the keys stay large and tappable.
    for (int c = 0; c < 4; ++c)
        grid->setColumnStretch(c, 1);
    for (int r = 0; r < 4; ++r)
        grid->setRowStretch(r, 1);
    root->addWidget(keys, 1);

    hide();
    qApp->installEventFilter(this);
}

QPushButton *NumpadOverlay::makeKey(const QString &label)
{
    auto *key = new QPushButton(label, this);
    key->setFocusPolicy(Qt::NoFocus);
    key->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    return key;
}

QAbstractSpinBox *NumpadOverlay::resolveTarget(QObject *obj) const
{
    auto *w = qobject_cast<QWidget *>(obj);
    if (!w)
        return nullptr;
    // A spin box focuses its internal line edit; edit the spin box behind it.
    if (qobject_cast<QLineEdit *>(w)) {
        if (auto *box = qobject_cast<QAbstractSpinBox *>(w->parentWidget()))
            return box;
        return nullptr; // a standalone text field: not ours to drive
    }
    return qobject_cast<QAbstractSpinBox *>(w);
}

bool NumpadOverlay::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::FocusIn: {
        // Only pop the pad open when the user actually reaches for a field
        // (tap, tab or shortcut). Focus a widget gets just because its screen
        // was shown or the window (re)activated must not open the pad — that is
        // what made the pad appear by default on a program's first entry.
        const auto reason = static_cast<QFocusEvent *>(event)->reason();
        const bool userDriven =
            reason == Qt::MouseFocusReason || reason == Qt::TabFocusReason ||
            reason == Qt::BacktabFocusReason || reason == Qt::ShortcutFocusReason;
        if (QAbstractSpinBox *box = userDriven ? resolveTarget(obj) : nullptr) {
            if (box->isEnabled() && !box->isReadOnly())
                attachTo(box);
        } else if (isVisible()) {
            // Focus moved to some other real widget we don't drive: dismiss the
            // pad. Ignore focus events targeting non-widget objects (e.g. the
            // QStyle or the backing QWindow), which fire as a side effect of
            // showing the pad and must not tear it back down.
            auto *w = qobject_cast<QWidget *>(obj);
            if (w && !isAncestorOf(w))
                detach();
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        // A tap outside both the pad and the edited field dismisses it (the
        // value is already committed live).
        if (isVisible()) {
            auto *w = qobject_cast<QWidget *>(obj);
            const bool onPad = w && (w == this || isAncestorOf(w));
            const bool onTarget = m_target && w &&
                                  (w == m_target || m_target->isAncestorOf(w));
            if (w && !onPad && !onTarget)
                detach();
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(obj, event);
}

void NumpadOverlay::attachTo(QAbstractSpinBox *target)
{
    m_target = target;
    m_buffer = plainValue(target);
    m_originalDisplay = target->text();
    m_dotKey->setEnabled(allowsDecimals(target));
    m_commaKey->setEnabled(allowsDecimals(target));
    m_signKey->setEnabled(allowsNegative(target));
    updateDisplay();
    reposition();
    show();
    raise();
}

void NumpadOverlay::detach()
{
    if (m_target)
        m_target->clearFocus(); // so tapping the same field re-opens the pad
    m_target = nullptr;
    hide();
}

void NumpadOverlay::appendText(const QString &text)
{
    if (!m_target)
        return;
    if (text == QLatin1String(".") || text == QLatin1String(",")) {
        if (!allowsDecimals(m_target) || m_buffer.contains(QLatin1Char('.')))
            return;
        // Leading decimal becomes "0." so the buffer always parses.
        if (m_buffer.isEmpty() || m_buffer == QLatin1String("-"))
            m_buffer += QLatin1Char('0');
        m_buffer += QLatin1Char('.');
    } else {
        m_buffer += text; // a digit
    }
    commit();
    updateDisplay();
}

void NumpadOverlay::backspace()
{
    if (!m_target || m_buffer.isEmpty())
        return;
    m_buffer.chop(1);
    commit();
    updateDisplay();
}

void NumpadOverlay::clearInput()
{
    if (!m_target)
        return;
    m_buffer.clear();
    updateDisplay();
}

void NumpadOverlay::toggleSign()
{
    if (!m_target || !allowsNegative(m_target))
        return;
    if (m_buffer.startsWith(QLatin1Char('-')))
        m_buffer.remove(0, 1);
    else
        m_buffer.prepend(QLatin1Char('-'));
    commit();
    updateDisplay();
}

void NumpadOverlay::commit()
{
    if (!m_target)
        return;
    // Skip intermediate states that don't parse to a number yet.
    bool ok = false;
    const double value = m_buffer.toDouble(&ok);
    if (!ok)
        return;
    if (auto *d = qobject_cast<QDoubleSpinBox *>(m_target.data()))
        d->setValue(value); // spin box clamps to its own range
    else if (auto *i = qobject_cast<QSpinBox *>(m_target.data()))
        i->setValue(qRound(value));
}

void NumpadOverlay::updateDisplay()
{
    m_currentLabel->setText(tr("Now  %1").arg(m_originalDisplay));
    QString shown = m_buffer.isEmpty() ? QStringLiteral("—") : m_buffer;
    if (auto *d = qobject_cast<QDoubleSpinBox *>(m_target.data()))
        shown = d->prefix() + shown + d->suffix();
    else if (auto *i = qobject_cast<QSpinBox *>(m_target.data()))
        shown = i->prefix() + shown + i->suffix();
    m_newLabel->setText(tr("New  %1").arg(shown));
}

void NumpadOverlay::reposition()
{
    const int h = static_cast<int>(m_host->height() * kHeightFraction);
    setGeometry(0, m_host->height() - h, m_host->width(), h);
}

void NumpadOverlay::showEvent(QShowEvent *event)
{
    reposition();
    QWidget::showEvent(event);
}

} // namespace duality
