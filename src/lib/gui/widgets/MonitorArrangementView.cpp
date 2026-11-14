/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorArrangementView.h"

#include "gui/config/MonitorArrangement.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPalette>

#include <algorithm>

namespace {
// pixels on screen within which a dragged edge snaps to another
constexpr double kSnapPixels = 10.0;
constexpr double kMargin = 24.0;

QColor computerColor(int index, const QPalette &palette)
{
  // spread hues apart, and keep them readable in light and dark themes
  const bool dark = palette.color(QPalette::Window).lightness() < 128;
  return QColor::fromHsv((index * 137 + 200) % 360, dark ? 110 : 70, dark ? 120 : 235);
}
} // namespace

MonitorArrangementView::MonitorArrangementView(QWidget *parent) : QWidget(parent)
{
  setMouseTracking(false);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setFocusPolicy(Qt::StrongFocus);
}

void MonitorArrangementView::setArrangement(MonitorArrangement *arrangement)
{
  m_arrangement = arrangement;
  refit();
}

void MonitorArrangementView::setSelectedComputer(const QString &name)
{
  if (m_selected == name) {
    return;
  }
  m_selected = name;
  update();
  Q_EMIT selectionChanged(name);
}

void MonitorArrangementView::refit()
{
  updateTransform();
  update();
}

QSize MonitorArrangementView::sizeHint() const
{
  return {640, 360};
}

QSize MonitorArrangementView::minimumSizeHint() const
{
  return {320, 180};
}

void MonitorArrangementView::updateTransform()
{
  m_toWidget = QTransform();
  if (m_arrangement == nullptr || m_arrangement->computers().isEmpty()) {
    return;
  }

  QRectF desk;
  for (const auto &c : m_arrangement->computers()) {
    desk = desk.isNull() ? c.deskBounds() : desk.united(c.deskBounds());
  }
  if (desk.width() <= 0.0 || desk.height() <= 0.0) {
    return;
  }

  // leave room around the desk so computers can be dragged outwards
  const double zoom = std::max(
      1e-4, std::min((width() - 2 * kMargin) / (desk.width() * 1.5), (height() - 2 * kMargin) / (desk.height() * 1.5))
  );
  const QPointF center = desk.center();
  m_toWidget.translate(width() / 2.0, height() / 2.0);
  m_toWidget.scale(zoom, zoom);
  m_toWidget.translate(-center.x(), -center.y());
}

void MonitorArrangementView::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), palette().color(QPalette::Base));

  if (m_arrangement == nullptr || m_arrangement->computers().isEmpty()) {
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(rect(), Qt::AlignCenter, tr("Add a computer to arrange it here."));
    return;
  }

  const auto &computers = m_arrangement->computers();
  const auto text = palette().color(QPalette::Text);
  const auto highlight = palette().color(QPalette::Highlight);

  for (int ci = 0; ci < computers.size(); ++ci) {
    const auto &c = computers.at(ci);
    const bool selected = c.name == m_selected;
    const auto fill = computerColor(ci, palette());

    for (int i = 0; i < c.monitors.size(); ++i) {
      const QRectF r = m_toWidget.mapRect(c.deskRect(i)).adjusted(1, 1, -1, -1);

      QPen pen(selected ? highlight : text.darker(120), selected ? 2.0 : 1.0);
      if (!c.reported) {
        pen.setStyle(Qt::DashLine);
      }
      painter.setPen(pen);
      painter.setBrush(fill);
      painter.drawRoundedRect(r, 3, 3);

      // monitor number, which is the "@N" in links
      painter.setPen(text);
      auto font = painter.font();
      font.setBold(true);
      painter.setFont(font);
      painter.drawText(r.adjusted(4, 2, -4, -2), Qt::AlignRight | Qt::AlignBottom, QString::number(i + 1));
    }

    // computer name over its first monitor
    if (!c.monitors.isEmpty()) {
      const QRectF r = m_toWidget.mapRect(c.deskRect(0)).adjusted(6, 4, -6, -4);
      auto font = painter.font();
      font.setBold(selected);
      painter.setFont(font);
      painter.setPen(text);
      auto label = c.name;
      if (!c.reported) {
        label += QStringLiteral("\n") + tr("(not seen yet)");
      }
      painter.drawText(r, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);
    }
  }

  // where the cursor can cross between computers
  // a warm color that stands out against the computers' cool and pastel fills
  QPen contactPen(QColor(235, 130, 20), 5.0, Qt::SolidLine, Qt::RoundCap);
  painter.setPen(contactPen);
  for (const auto &line : m_arrangement->contacts()) {
    painter.drawLine(m_toWidget.map(line));
  }

  if (m_arrangement->hasOverlaps()) {
    painter.setPen(QColor(Qt::red));
    painter.drawText(
        rect().adjusted(8, 8, -8, -8), Qt::AlignLeft | Qt::AlignBottom,
        tr("Some monitors overlap. Move them apart so each edge only touches one neighbor.")
    );
  }
}

QString MonitorArrangementView::computerAt(QPointF widgetPos) const
{
  if (m_arrangement == nullptr) {
    return {};
  }
  const auto desk = m_toWidget.inverted().map(widgetPos);
  // later computers are drawn on top, so check them first
  const auto &computers = m_arrangement->computers();
  for (auto c = computers.crbegin(); c != computers.crend(); ++c) {
    for (int i = 0; i < c->monitors.size(); ++i) {
      if (c->deskRect(i).contains(desk)) {
        return c->name;
      }
    }
  }
  return {};
}

void MonitorArrangementView::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || m_arrangement == nullptr) {
    QWidget::mousePressEvent(event);
    return;
  }
  const auto name = computerAt(event->position());
  setSelectedComputer(name);
  if (const auto *c = m_arrangement->computer(name); c != nullptr) {
    m_dragging = name;
    m_dragOffset = c->position - m_toWidget.inverted().map(event->position());
    m_moved = false;
  }
}

void MonitorArrangementView::mouseMoveEvent(QMouseEvent *event)
{
  if (m_dragging.isEmpty() || m_arrangement == nullptr) {
    return;
  }
  // the zoom stays fixed while dragging so the computer follows the mouse
  const auto desk = m_toWidget.inverted().map(event->position());
  const double snap = (event->modifiers() & Qt::ShiftModifier) ? 0.0 : kSnapPixels / m_toWidget.m11();
  m_arrangement->moveComputer(m_dragging, desk + m_dragOffset, snap);
  m_moved = true;
  update();
  Q_EMIT arrangementChanged();
}

void MonitorArrangementView::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && !m_dragging.isEmpty()) {
    m_dragging.clear();
    if (m_moved) {
      refit();
    }
  }
}

void MonitorArrangementView::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }
  m_dragging.clear();
  if (const auto name = computerAt(event->position()); !name.isEmpty()) {
    setSelectedComputer(name);
    Q_EMIT computerActivated(name);
  }
}

void MonitorArrangementView::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  updateTransform();
}
