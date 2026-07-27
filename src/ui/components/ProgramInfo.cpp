#include "ui/components/ProgramInfo.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace duality {

ProgramInfo::ProgramInfo(QWidget *parent) : QToolButton(parent) {
  setIcon(QIcon(QStringLiteral(":/assets/info.png")));
  setIconSize(QSize(15, 15));
  setCursor(Qt::PointingHandCursor);
  setToolTip(tr("About this program"));
  setFixedSize(24, 24);
  // The info glyph is black, so give the button a white face (like the home
  // tiles) to make it read on the dark top bar.
  setStyleSheet(QStringLiteral(
      "QToolButton { background: #ffffff; border: 1px solid #808080;"
      " padding: 0px; }"
      "QToolButton:pressed { background: #d0d0d0; }"));
  hide();
  connect(this, &QToolButton::clicked, this, &ProgramInfo::openDialog);
}

void ProgramInfo::setInfo(const QString &title, const QString &body) {
  m_title = title;
  m_body = body;
  setVisible(!body.isEmpty());
}

void ProgramInfo::openDialog() {
  if (m_body.isEmpty())
    return;

  // Frameless modal sized to sit inside the fixed 320x480 popout, centered on
  // the window. The global stylesheet already themes the scroll area/bars.
  QDialog dlg(window());
  dlg.setModal(true);
  dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  dlg.setFixedSize(300, 440);
  dlg.setStyleSheet(
      QStringLiteral("QDialog { border: 1px solid #ffffff; }"));

  auto *root = new QVBoxLayout(&dlg);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  // Inverted header strip: program title on the left, close button on the
  // right (matches the app's white-on-black title convention).
  auto *header = new QWidget(&dlg);
  header->setObjectName(QStringLiteral("infoHeader"));
  header->setFixedHeight(30);
  header->setStyleSheet(QStringLiteral(
      "#infoHeader { background: #ffffff; }"
      "#infoHeader QLabel { background: transparent; color: #000000;"
      " font-weight: bold; }"));
  auto *headerRow = new QHBoxLayout(header);
  headerRow->setContentsMargins(8, 2, 4, 2);
  auto *heading = new QLabel(m_title, header);
  headerRow->addWidget(heading);
  headerRow->addStretch(1);
  auto *close = new QToolButton(header);
  close->setText(QStringLiteral("✕"));
  close->setCursor(Qt::PointingHandCursor);
  close->setFixedSize(24, 24);
  close->setStyleSheet(QStringLiteral(
      "QToolButton { background: #ffffff; color: #000000; border: none;"
      " font-weight: bold; }"
      "QToolButton:pressed { background: #d0d0d0; }"));
  connect(close, &QToolButton::clicked, &dlg, &QDialog::accept);
  headerRow->addWidget(close);
  root->addWidget(header);

  // Scrollable body: rich text that wraps and grows; the scroll bar appears
  // only when the content overflows.
  auto *scroll = new QScrollArea(&dlg);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *body = new QLabel(m_body, scroll);
  body->setTextFormat(Qt::RichText);
  body->setWordWrap(true);
  body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  body->setTextInteractionFlags(Qt::TextSelectableByMouse);
  body->setContentsMargins(10, 10, 10, 10);
  scroll->setWidget(body);
  root->addWidget(scroll, 1);

  dlg.exec();
}

} // namespace duality
