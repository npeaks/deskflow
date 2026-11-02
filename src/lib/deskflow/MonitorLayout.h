/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deskflow {

//! A single monitor, in the owning computer's global desktop coordinates
struct MonitorRect
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;

  bool contains(int32_t px, int32_t py) const
  {
    return px >= x && px < x + w && py >= y && py < y + h;
  }

  bool isValid() const
  {
    return w > 0 && h > 0;
  }

  bool operator==(const MonitorRect &) const = default;
};

using MonitorList = std::vector<MonitorRect>;

/*!
Geometry helpers for computers with more than one monitor.

Monitors are numbered for the user (in config files and logs) starting
at 1, in canonical order: sorted by left edge, then by top edge.  So on
a desk of side-by-side monitors, monitor 1 is the leftmost.
*/
namespace monitors {

//! Drop invalid rects and sort into canonical (numbering) order
void normalize(MonitorList &monitors);

//! Smallest rect enclosing all monitors (empty rect if there are none)
MonitorRect bounds(const MonitorList &monitors);

//! Index of the monitor containing the point, or -1
int find(const MonitorList &monitors, int32_t x, int32_t y);

//! Index of the monitor closest to the point (containing it if any), or -1 if empty
int nearest(const MonitorList &monitors, int32_t x, int32_t y);

//! Clamp a point onto a monitor
void clamp(const MonitorRect &monitor, int32_t &x, int32_t &y);

/*!
Returns true if moving past edge \p side of monitor \p index at the point
(\p x, \p y) leaves the desktop, i.e. the pixel just beyond that edge is not
on any monitor.  The point is clamped onto the monitor first.
*/
bool isExposed(const MonitorList &monitors, int index, Direction side, int32_t x, int32_t y);

/*!
Returns true if edge \p side of \p monitor lies on the same side of the
bounding box, so the old whole-computer edge logic applies to it.
*/
bool isOnBoundsEdge(const MonitorList &monitors, const MonitorRect &monitor, Direction side);

//! Position of a point along edge \p side of the monitor, as a fraction in [0,1)
float toFraction(const MonitorRect &monitor, Direction side, int32_t x, int32_t y);

//! Point on edge \p side of the monitor at fraction \p f along it
void fromFraction(const MonitorRect &monitor, Direction side, float f, int32_t &x, int32_t &y);

//! Human readable list for logs, e.g. "1: 0,0 2160x3840; 2: 2160,0 3840x2160"
std::string toString(const MonitorList &monitors);

} // namespace monitors
} // namespace deskflow
