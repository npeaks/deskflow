/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/MonitorLayout.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace deskflow::monitors {

void normalize(MonitorList &monitors)
{
  std::erase_if(monitors, [](const MonitorRect &m) { return !m.isValid(); });
  std::ranges::stable_sort(monitors, [](const MonitorRect &a, const MonitorRect &b) {
    if (a.x != b.x) {
      return a.x < b.x;
    }
    return a.y < b.y;
  });
}

MonitorRect bounds(const MonitorList &monitors)
{
  if (monitors.empty()) {
    return {};
  }
  int32_t x0 = std::numeric_limits<int32_t>::max();
  int32_t y0 = std::numeric_limits<int32_t>::max();
  int32_t x1 = std::numeric_limits<int32_t>::min();
  int32_t y1 = std::numeric_limits<int32_t>::min();
  for (const auto &m : monitors) {
    x0 = std::min(x0, m.x);
    y0 = std::min(y0, m.y);
    x1 = std::max(x1, m.x + m.w);
    y1 = std::max(y1, m.y + m.h);
  }
  return {x0, y0, x1 - x0, y1 - y0};
}

int find(const MonitorList &monitors, int32_t x, int32_t y)
{
  for (size_t i = 0; i < monitors.size(); ++i) {
    if (monitors[i].contains(x, y)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int nearest(const MonitorList &monitors, int32_t x, int32_t y)
{
  int best = -1;
  int64_t bestDistance = std::numeric_limits<int64_t>::max();
  for (size_t i = 0; i < monitors.size(); ++i) {
    int32_t cx = x;
    int32_t cy = y;
    clamp(monitors[i], cx, cy);
    const int64_t dx = static_cast<int64_t>(cx) - x;
    const int64_t dy = static_cast<int64_t>(cy) - y;
    const int64_t distance = dx * dx + dy * dy;
    if (distance < bestDistance) {
      bestDistance = distance;
      best = static_cast<int>(i);
    }
  }
  return best;
}

void clamp(const MonitorRect &monitor, int32_t &x, int32_t &y)
{
  x = std::clamp(x, monitor.x, monitor.x + monitor.w - 1);
  y = std::clamp(y, monitor.y, monitor.y + monitor.h - 1);
}

bool isExposed(const MonitorList &monitors, int index, Direction side, int32_t x, int32_t y)
{
  if (index < 0 || index >= static_cast<int>(monitors.size())) {
    return false;
  }
  const auto &m = monitors[index];
  clamp(m, x, y);
  switch (side) {
    using enum Direction;
  case Left:
    x = m.x - 1;
    break;
  case Right:
    x = m.x + m.w;
    break;
  case Top:
    y = m.y - 1;
    break;
  case Bottom:
    y = m.y + m.h;
    break;
  case NoDirection:
    return false;
  }
  return find(monitors, x, y) == -1;
}

bool isOnBoundsEdge(const MonitorList &monitors, const MonitorRect &monitor, Direction side)
{
  const auto b = bounds(monitors);
  switch (side) {
    using enum Direction;
  case Left:
    return monitor.x == b.x;
  case Right:
    return monitor.x + monitor.w == b.x + b.w;
  case Top:
    return monitor.y == b.y;
  case Bottom:
    return monitor.y + monitor.h == b.y + b.h;
  case NoDirection:
    break;
  }
  return false;
}

float toFraction(const MonitorRect &monitor, Direction side, int32_t x, int32_t y)
{
  clamp(monitor, x, y);
  switch (side) {
    using enum Direction;
  case Left:
  case Right:
    return (static_cast<float>(y - monitor.y) + 0.5f) / static_cast<float>(monitor.h);
  case Top:
  case Bottom:
    return (static_cast<float>(x - monitor.x) + 0.5f) / static_cast<float>(monitor.w);
  case NoDirection:
    break;
  }
  return 0.0f;
}

void fromFraction(const MonitorRect &monitor, Direction side, float f, int32_t &x, int32_t &y)
{
  f = std::clamp(f, 0.0f, 1.0f);
  switch (side) {
    using enum Direction;
  case Left:
    x = monitor.x;
    y = monitor.y + static_cast<int32_t>(f * static_cast<float>(monitor.h));
    break;
  case Right:
    x = monitor.x + monitor.w - 1;
    y = monitor.y + static_cast<int32_t>(f * static_cast<float>(monitor.h));
    break;
  case Top:
    x = monitor.x + static_cast<int32_t>(f * static_cast<float>(monitor.w));
    y = monitor.y;
    break;
  case Bottom:
    x = monitor.x + static_cast<int32_t>(f * static_cast<float>(monitor.w));
    y = monitor.y + monitor.h - 1;
    break;
  case NoDirection:
    break;
  }
  clamp(monitor, x, y);
}

std::string toString(const MonitorList &monitors)
{
  std::ostringstream s;
  for (size_t i = 0; i < monitors.size(); ++i) {
    const auto &m = monitors[i];
    if (i > 0) {
      s << "; ";
    }
    s << (i + 1) << ": " << m.x << "," << m.y << " " << m.w << "x" << m.h;
  }
  return s.str();
}

} // namespace deskflow::monitors
