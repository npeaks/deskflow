/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorArrangement.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

enum class Side
{
  Left,
  Right,
  Up,
  Down
};

const char *sideName(Side side)
{
  switch (side) {
    using enum Side;
  case Left:
    return "left";
  case Right:
    return "right";
  case Up:
    return "up";
  case Down:
    return "down";
  }
  return "";
}

// a percentage the server config parser accepts, with no needless digits
QString formatPercent(double value)
{
  auto text = QString::number(value, 'f', 2);
  while (text.endsWith('0')) {
    text.chop(1);
  }
  if (text.endsWith('.')) {
    text.chop(1);
  }
  return text;
}

QString formatInterval(double start, double end)
{
  if (start <= 0.0 && end >= 100.0) {
    return {};
  }
  return QStringLiteral("(%1,%2)").arg(formatPercent(start), formatPercent(end));
}

double roundPercent(double value)
{
  return std::clamp(std::round(value * 100.0) / 100.0, 0.0, 100.0);
}

// the piece of edge \p side of \p from that touches \p to, as percentages
// along each rect's edge.  returns false if they don't touch there.
bool touchingInterval(
    const QRectF &from, const QRectF &to, Side side, double &fromStart, double &fromEnd, double &toStart, double &toEnd
)
{
  using enum Side;
  const double tol = MonitorArrangement::kTouchTolerance;

  double gap = 0.0;
  switch (side) {
  case Left:
    gap = from.left() - to.right();
    break;
  case Right:
    gap = to.left() - from.right();
    break;
  case Up:
    gap = from.top() - to.bottom();
    break;
  case Down:
    gap = to.top() - from.bottom();
    break;
  }
  if (std::abs(gap) > tol) {
    return false;
  }

  const bool horizontal = side == Left || side == Right;
  const double fromLo = horizontal ? from.top() : from.left();
  const double fromLen = horizontal ? from.height() : from.width();
  const double toLo = horizontal ? to.top() : to.left();
  const double toLen = horizontal ? to.height() : to.width();

  const double lo = std::max(fromLo, toLo);
  const double hi = std::min(fromLo + fromLen, toLo + toLen);
  if (hi - lo <= tol || fromLen <= 0.0 || toLen <= 0.0) {
    return false;
  }

  fromStart = roundPercent((lo - fromLo) / fromLen * 100.0);
  fromEnd = roundPercent((hi - fromLo) / fromLen * 100.0);
  toStart = roundPercent((lo - toLo) / toLen * 100.0);
  toEnd = roundPercent((hi - toLo) / toLen * 100.0);
  return fromStart < fromEnd && toStart < toEnd;
}

} // namespace

//
// ArrangedComputer
//

QRect ArrangedComputer::bounds() const
{
  QRect b;
  for (const auto &m : monitors) {
    b = b.isNull() ? m : b.united(m);
  }
  return b;
}

QRectF ArrangedComputer::deskRect(int index) const
{
  const auto origin = bounds().topLeft();
  const auto &m = monitors.at(index);
  return {
      position.x() + (m.x() - origin.x()) * scale, position.y() + (m.y() - origin.y()) * scale, m.width() * scale,
      m.height() * scale
  };
}

QRectF ArrangedComputer::deskBounds() const
{
  const auto b = bounds();
  return {position.x(), position.y(), b.width() * scale, b.height() * scale};
}

//
// MonitorArrangement
//

ArrangedComputer *MonitorArrangement::computer(const QString &name)
{
  for (auto &c : m_computers) {
    if (c.name == name) {
      return &c;
    }
  }
  return nullptr;
}

const ArrangedComputer *MonitorArrangement::computer(const QString &name) const
{
  for (const auto &c : m_computers) {
    if (c.name == name) {
      return &c;
    }
  }
  return nullptr;
}

void MonitorArrangement::setComputers(const QStringList &names, const QHash<QString, QString> &monitors)
{
  QList<ArrangedComputer> computers;
  for (const auto &name : names) {
    if (name.isEmpty()) {
      continue;
    }
    if (const auto *existing = computer(name); existing != nullptr) {
      computers.append(*existing);
      continue;
    }
    ArrangedComputer c;
    c.name = name;
    c.monitors = parseMonitors(monitors.value(name));
    c.reported = !c.monitors.isEmpty();
    if (!c.reported) {
      c.monitors = {kPlaceholderMonitor};
    }
    computers.append(c);
  }
  m_computers = computers;
}

void MonitorArrangement::setMonitors(const QString &name, const QList<QRect> &monitors)
{
  auto *c = computer(name);
  if (c == nullptr || monitors.isEmpty()) {
    return;
  }
  c->monitors = monitors;
  c->reported = true;
}

QPointF MonitorArrangement::moveComputer(const QString &name, QPointF position, double snapDistance)
{
  auto *c = computer(name);
  if (c == nullptr) {
    return position;
  }
  c->position = position;
  c->placed = true;

  if (snapDistance <= 0.0) {
    return position;
  }

  // find the smallest nudge in each direction that lines up one of our
  // monitor edges with a nearby monitor edge of another computer
  double bestDx = std::numeric_limits<double>::max();
  double bestDy = std::numeric_limits<double>::max();
  const auto consider = [snapDistance](double &best, double delta) {
    if (std::abs(delta) <= snapDistance && std::abs(delta) < std::abs(best)) {
      best = delta;
    }
  };

  for (int i = 0; i < c->monitors.size(); ++i) {
    const auto mine = c->deskRect(i);
    for (const auto &other : std::as_const(m_computers)) {
      if (other.name == c->name || !other.placed) {
        continue;
      }
      for (int j = 0; j < other.monitors.size(); ++j) {
        const auto theirs = other.deskRect(j);
        // only snap to monitors that are close in the other direction too
        const bool nearVertically =
            mine.top() <= theirs.bottom() + snapDistance && theirs.top() <= mine.bottom() + snapDistance;
        const bool nearHorizontally =
            mine.left() <= theirs.right() + snapDistance && theirs.left() <= mine.right() + snapDistance;
        if (nearVertically) {
          consider(bestDx, theirs.right() - mine.left());
          consider(bestDx, theirs.left() - mine.right());
          consider(bestDx, theirs.left() - mine.left());
          consider(bestDx, theirs.right() - mine.right());
        }
        if (nearHorizontally) {
          consider(bestDy, theirs.bottom() - mine.top());
          consider(bestDy, theirs.top() - mine.bottom());
          consider(bestDy, theirs.top() - mine.top());
          consider(bestDy, theirs.bottom() - mine.bottom());
        }
      }
    }
  }

  if (bestDx != std::numeric_limits<double>::max()) {
    c->position.rx() += bestDx;
  }
  if (bestDy != std::numeric_limits<double>::max()) {
    c->position.ry() += bestDy;
  }
  return c->position;
}

void MonitorArrangement::setScale(const QString &name, double scale)
{
  if (auto *c = computer(name); c != nullptr && scale > 0.0) {
    c->scale = scale;
  }
}

void MonitorArrangement::renameComputer(const QString &from, const QString &to)
{
  if (from == to || computer(to) != nullptr) {
    return;
  }
  if (auto *c = computer(from); c != nullptr) {
    c->name = to;
  }
}

void MonitorArrangement::resetPlacements()
{
  for (auto &c : m_computers) {
    c.placed = false;
  }
}

void MonitorArrangement::placeUnplaced(const QStringList &order)
{
  // start to the right of everything already placed
  double x = 0.0;
  bool anyPlaced = false;
  for (const auto &c : std::as_const(m_computers)) {
    if (c.placed) {
      x = anyPlaced ? std::max(x, c.deskBounds().right()) : c.deskBounds().right();
      anyPlaced = true;
    }
  }

  // unplaced computers sit side by side, touching, so they're linked
  // left to right until the user arranges them
  QStringList names = order;
  for (const auto &c : std::as_const(m_computers)) {
    if (!names.contains(c.name)) {
      names.append(c.name);
    }
  }
  for (const auto &name : std::as_const(names)) {
    auto *c = computer(name);
    if (c == nullptr || c->placed) {
      continue;
    }
    c->position = {x, 0.0};
    c->placed = true;
    x += c->deskBounds().width();
  }
}

bool MonitorArrangement::hasOverlaps() const
{
  for (int a = 0; a < m_computers.size(); ++a) {
    for (int b = a + 1; b < m_computers.size(); ++b) {
      const auto &ca = m_computers.at(a);
      const auto &cb = m_computers.at(b);
      for (int i = 0; i < ca.monitors.size(); ++i) {
        for (int j = 0; j < cb.monitors.size(); ++j) {
          const auto overlap = ca.deskRect(i).intersected(cb.deskRect(j));
          if (overlap.width() > kTouchTolerance && overlap.height() > kTouchTolerance) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

QList<QLineF> MonitorArrangement::contacts() const
{
  QList<QLineF> lines;
  const double tol = kTouchTolerance;
  for (int a = 0; a < m_computers.size(); ++a) {
    for (int b = a + 1; b < m_computers.size(); ++b) {
      const auto &ca = m_computers.at(a);
      const auto &cb = m_computers.at(b);
      for (int i = 0; i < ca.monitors.size(); ++i) {
        const auto r1 = ca.deskRect(i);
        for (int j = 0; j < cb.monitors.size(); ++j) {
          const auto r2 = cb.deskRect(j);
          const double top = std::max(r1.top(), r2.top());
          const double bottom = std::min(r1.bottom(), r2.bottom());
          const double left = std::max(r1.left(), r2.left());
          const double right = std::min(r1.right(), r2.right());
          if (bottom - top > tol) {
            if (std::abs(r1.right() - r2.left()) <= tol) {
              lines.append({r1.right(), top, r1.right(), bottom});
            } else if (std::abs(r1.left() - r2.right()) <= tol) {
              lines.append({r1.left(), top, r1.left(), bottom});
            }
          }
          if (right - left > tol) {
            if (std::abs(r1.bottom() - r2.top()) <= tol) {
              lines.append({left, r1.bottom(), right, r1.bottom()});
            } else if (std::abs(r1.top() - r2.bottom()) <= tol) {
              lines.append({left, r1.top(), right, r1.top()});
            }
          }
        }
      }
    }
  }
  return lines;
}

QStringList MonitorArrangement::links(const QString &name) const
{
  const auto *from = computer(name);
  if (from == nullptr) {
    return {};
  }

  // links on the same monitor and side can't overlap or the server
  // rejects the config, which could only happen if monitors overlap on
  // the desk.  keep the first and drop the rest.
  struct Used
  {
    int monitor;
    Side side;
    double start;
    double end;
  };
  QList<Used> used;

  QStringList lines;
  for (int i = 0; i < from->monitors.size(); ++i) {
    const auto mine = from->deskRect(i);
    for (const auto side : {Side::Left, Side::Right, Side::Up, Side::Down}) {
      for (const auto &to : std::as_const(m_computers)) {
        if (to.name == from->name) {
          continue;
        }
        for (int j = 0; j < to.monitors.size(); ++j) {
          double fromStart;
          double fromEnd;
          double toStart;
          double toEnd;
          if (!touchingInterval(mine, to.deskRect(j), side, fromStart, fromEnd, toStart, toEnd)) {
            continue;
          }
          const bool overlapsUsed = std::ranges::any_of(used, [&](const Used &u) {
            return u.monitor == i && u.side == side && fromStart < u.end && u.start < fromEnd;
          });
          if (overlapsUsed) {
            continue;
          }
          used.append({i, side, fromStart, fromEnd});
          lines.append(QStringLiteral("%1@%2%3 = %4@%5%6")
                           .arg(sideName(side))
                           .arg(i + 1)
                           .arg(formatInterval(fromStart, fromEnd), to.name)
                           .arg(j + 1)
                           .arg(formatInterval(toStart, toEnd)));
        }
      }
    }
  }
  return lines;
}

QList<QRect> MonitorArrangement::parseMonitors(const QString &text)
{
  QList<QRect> monitors;
  for (const auto &entry : text.split(';', Qt::SkipEmptyParts)) {
    const auto parts = entry.split(',');
    if (parts.size() != 4) {
      continue;
    }
    bool ok[4] = {};
    const QRect r(parts[0].toInt(&ok[0]), parts[1].toInt(&ok[1]), parts[2].toInt(&ok[2]), parts[3].toInt(&ok[3]));
    if (ok[0] && ok[1] && ok[2] && ok[3] && r.width() > 0 && r.height() > 0) {
      monitors.append(r);
    }
  }
  // the same order the server numbers them in
  std::ranges::stable_sort(monitors, [](const QRect &a, const QRect &b) {
    return a.x() != b.x() ? a.x() < b.x() : a.y() < b.y();
  });
  return monitors;
}

QString MonitorArrangement::formatMonitors(const QList<QRect> &monitors)
{
  QStringList entries;
  for (const auto &m : monitors) {
    entries.append(QStringLiteral("%1,%2,%3,%4").arg(m.x()).arg(m.y()).arg(m.width()).arg(m.height()));
  }
  return entries.join(';');
}

bool MonitorArrangement::parsePlacement(const QString &text, QPointF &position, double &scale)
{
  const auto parts = text.split(',');
  if (parts.size() != 3) {
    return false;
  }
  bool ok[3] = {};
  const double x = parts[0].toDouble(&ok[0]);
  const double y = parts[1].toDouble(&ok[1]);
  const double s = parts[2].toDouble(&ok[2]);
  if (!ok[0] || !ok[1] || !ok[2] || s <= 0.0) {
    return false;
  }
  position = {x, y};
  scale = s;
  return true;
}

QString MonitorArrangement::formatPlacement(QPointF position, double scale)
{
  return QStringLiteral("%1,%2,%3").arg(position.x()).arg(position.y()).arg(scale);
}
