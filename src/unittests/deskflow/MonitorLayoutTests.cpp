/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorLayoutTests.h"

#include "deskflow/MonitorLayout.h"

using namespace deskflow;
using enum Direction;

namespace {
// a portrait 4k with a landscape 4k to its right, tops aligned:
//
//   +-----++-----------+
//   |     ||     2     |
//   |  1  |+-----------+
//   |     |   (no monitor)
//   +-----+
const MonitorRect kPortrait{0, 0, 2160, 3840};
const MonitorRect kLandscape{2160, 0, 3840, 2160};
const MonitorList kDesk{kPortrait, kLandscape};
} // namespace

void MonitorLayoutTests::normalize_sortsByLeftThenTop_dropsInvalid()
{
  MonitorList list{{100, 50, 10, 10}, {0, 0, 0, 10}, {100, 0, 10, 10}, {-50, 0, 10, 10}};
  monitors::normalize(list);

  QCOMPARE(list.size(), size_t{3});
  QCOMPARE(list[0], (MonitorRect{-50, 0, 10, 10}));
  QCOMPARE(list[1], (MonitorRect{100, 0, 10, 10}));
  QCOMPARE(list[2], (MonitorRect{100, 50, 10, 10}));
}

void MonitorLayoutTests::bounds_enclosesAll()
{
  QCOMPARE(monitors::bounds(kDesk), (MonitorRect{0, 0, 6000, 3840}));
  QCOMPARE(monitors::bounds({}), MonitorRect{});
}

void MonitorLayoutTests::find_pointOnMonitor()
{
  QCOMPARE(monitors::find(kDesk, 10, 3000), 0);
  QCOMPARE(monitors::find(kDesk, 2160, 0), 1);
  QCOMPARE(monitors::find(kDesk, 5999, 2159), 1);
}

void MonitorLayoutTests::find_pointInGap_none()
{
  QCOMPARE(monitors::find(kDesk, 3000, 2160), -1);
  QCOMPARE(monitors::find(kDesk, 6000, 0), -1);
}

void MonitorLayoutTests::nearest_pointInGap_closestMonitor()
{
  // just right of the portrait's lower half
  QCOMPARE(monitors::nearest(kDesk, 2200, 3000), 0);
  // just under the landscape
  QCOMPARE(monitors::nearest(kDesk, 4000, 2200), 1);
  QCOMPARE(monitors::nearest({}, 0, 0), -1);
}

void MonitorLayoutTests::isExposed_sharedEdge_false()
{
  // the top of the portrait's right edge touches the landscape
  QVERIFY(!monitors::isExposed(kDesk, 0, Right, 2159, 100));
  QVERIFY(!monitors::isExposed(kDesk, 1, Left, 2160, 100));
}

void MonitorLayoutTests::isExposed_openEdge_true()
{
  // the bottom of the portrait's right edge is open
  QVERIFY(monitors::isExposed(kDesk, 0, Right, 2159, 3000));
  // so is the bottom of the landscape
  QVERIFY(monitors::isExposed(kDesk, 1, Bottom, 4000, 2159));
  // and the outer edges
  QVERIFY(monitors::isExposed(kDesk, 0, Left, 0, 100));
  QVERIFY(monitors::isExposed(kDesk, 1, Top, 4000, 0));
  QVERIFY(!monitors::isExposed(kDesk, 5, Top, 0, 0));
}

void MonitorLayoutTests::isOnBoundsEdge_innerEdge_false()
{
  QVERIFY(!monitors::isOnBoundsEdge(kDesk, kLandscape, Bottom));
  QVERIFY(!monitors::isOnBoundsEdge(kDesk, kPortrait, Right));
  QVERIFY(monitors::isOnBoundsEdge(kDesk, kPortrait, Bottom));
  QVERIFY(monitors::isOnBoundsEdge(kDesk, kLandscape, Right));
  QVERIFY(monitors::isOnBoundsEdge(kDesk, kLandscape, Top));
}

void MonitorLayoutTests::fraction_roundTrip()
{
  const float f = monitors::toFraction(kLandscape, Bottom, 2160 + 960, 2159);
  QVERIFY(f > 0.24f && f < 0.26f);

  int32_t x = 0;
  int32_t y = 0;
  monitors::fromFraction(kLandscape, Bottom, f, x, y);
  QCOMPARE(x, 2160 + 960);
  QCOMPARE(y, 2159);

  monitors::fromFraction(kPortrait, Right, 0.75f, x, y);
  QCOMPARE(x, 2159);
  QCOMPARE(y, 2880);
}

void MonitorLayoutTests::fromFraction_clampsOntoMonitor()
{
  int32_t x = 0;
  int32_t y = 0;
  monitors::fromFraction(kPortrait, Left, 1.0f, x, y);
  QCOMPARE(x, 0);
  QCOMPARE(y, 3839);

  monitors::fromFraction(kLandscape, Top, -1.0f, x, y);
  QCOMPARE(x, 2160);
  QCOMPARE(y, 0);
}

void MonitorLayoutTests::toString_numbersFromOne()
{
  QCOMPARE(monitors::toString(kDesk), std::string("1: 0,0 2160x3840; 2: 2160,0 3840x2160"));
}

QTEST_MAIN(MonitorLayoutTests)
