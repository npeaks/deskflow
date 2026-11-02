/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorArrangementTests.h"

#include "gui/config/MonitorArrangement.h"

namespace {
// a portrait 4k (1) and a landscape 4k (2) with tops aligned on "win", and
// a laptop tucked under the landscape monitor, against the portrait one
MonitorArrangement desk()
{
  MonitorArrangement a;
  a.setEnabled(true);
  a.setComputers({"win", "mac"}, {{"win", "2160,0,3840,2160;0,0,2160,3840"}, {"mac", "0,0,1512,982"}});
  a.moveComputer("win", {0, 0}, 0);
  a.moveComputer("mac", {2160, 2160}, 0);
  return a;
}
} // namespace

void MonitorArrangementTests::links_desk()
{
  const auto a = desk();
  QCOMPARE(a.links("win"), QStringList({"right@1(56.25,81.82) = mac@1", "down@2(0,39.38) = mac@1"}));
  QCOMPARE(a.links("mac"), QStringList({"left@1 = win@1(56.25,81.82)", "up@1 = win@2(0,39.38)"}));
  QVERIFY(a.links("nobody").isEmpty());
}

void MonitorArrangementTests::links_gap_none()
{
  auto a = desk();
  a.moveComputer("mac", {2200, 2200}, 0);
  QVERIFY(a.links("win").isEmpty());
  QVERIFY(a.links("mac").isEmpty());
}

void MonitorArrangementTests::links_sameComputer_none()
{
  MonitorArrangement a;
  a.setComputers({"win"}, {{"win", "2160,0,3840,2160;0,0,2160,3840"}});
  a.moveComputer("win", {0, 0}, 0);
  QVERIFY(a.links("win").isEmpty());
}

void MonitorArrangementTests::links_overlappingMonitors_intervalsDontOverlap()
{
  MonitorArrangement a;
  a.setComputers({"a", "b", "c"}, {{"a", "0,0,100,100"}, {"b", "0,0,100,100"}, {"c", "0,0,100,100"}});
  a.moveComputer("a", {0, 0}, 0);
  a.moveComputer("b", {100, 0}, 0);
  a.moveComputer("c", {100, 50}, 0); // overlaps b
  QVERIFY(a.hasOverlaps());

  // the server rejects overlapping ranges on one edge, so only one survives
  const auto links = a.links("a");
  QCOMPARE(links.size(), 1);
  QCOMPARE(links.first(), QStringLiteral("right@1 = b@1"));
}

void MonitorArrangementTests::links_scaled()
{
  MonitorArrangement a;
  a.setComputers({"big", "small"}, {{"big", "0,0,2000,1000"}, {"small", "0,0,1000,500"}});
  a.moveComputer("big", {0, 0}, 0);
  a.setScale("small", 2.0); // its pixels are twice the size, so it's as big as "big"
  a.moveComputer("small", {2000, 0}, 0);
  QCOMPARE(a.links("big"), QStringList({"right@1 = small@1"}));
}

void MonitorArrangementTests::contacts_desk()
{
  const auto contacts = desk().contacts();
  QCOMPARE(contacts.size(), 2);
  QVERIFY(contacts.contains(QLineF(2160, 2160, 2160, 3142)));
  QVERIFY(contacts.contains(QLineF(2160, 2160, 3672, 2160)));
}

void MonitorArrangementTests::moveComputer_snapsToNearbyEdge()
{
  auto a = desk();
  const auto snapped = a.moveComputer("mac", {2170, 2150}, 60);
  QCOMPARE(snapped, QPointF(2160, 2160));
  QCOMPARE(a.links("mac").size(), 2);
}

void MonitorArrangementTests::moveComputer_noSnap()
{
  auto a = desk();
  QCOMPARE(a.moveComputer("mac", {2170, 2150}, 0), QPointF(2170, 2150));
}

void MonitorArrangementTests::hasOverlaps()
{
  auto a = desk();
  QVERIFY(!a.hasOverlaps());
  a.moveComputer("mac", {2000, 2000}, 0);
  QVERIFY(a.hasOverlaps());
}

void MonitorArrangementTests::setComputers_keepsExisting_placeholderForUnknown()
{
  auto a = desk();
  a.setComputers({"mac", "fresh"}, {{"mac", "0,0,10,10"}});

  QCOMPARE(a.computers().size(), 2);
  const auto *mac = a.computer("mac");
  QVERIFY(mac != nullptr);
  QCOMPARE(mac->position, QPointF(2160, 2160));
  QCOMPARE(mac->monitors, QList<QRect>({QRect(0, 0, 1512, 982)}));

  const auto *fresh = a.computer("fresh");
  QVERIFY(fresh != nullptr);
  QVERIFY(!fresh->reported);
  QVERIFY(!fresh->placed);
  QCOMPARE(fresh->monitors, QList<QRect>({MonitorArrangement::kPlaceholderMonitor}));
  QVERIFY(a.computer("win") == nullptr);
}

void MonitorArrangementTests::placeUnplaced_rowTouching()
{
  MonitorArrangement a;
  a.setComputers({"a", "b"}, {{"a", "0,0,100,50"}, {"b", "0,0,80,50"}});
  a.placeUnplaced({"b", "a"});

  QCOMPARE(a.computer("b")->position, QPointF(0, 0));
  QCOMPARE(a.computer("a")->position, QPointF(80, 0));
  QCOMPARE(a.links("b"), QStringList({"right@1 = a@1"}));
}

void MonitorArrangementTests::parseMonitors_sortsAndDropsInvalid()
{
  const auto monitors = MonitorArrangement::parseMonitors("100,0,10,10;junk;0,5,10,10;0,0,0,10;0,0,10,10;1,2,3");
  QCOMPARE(monitors, QList<QRect>({QRect(0, 0, 10, 10), QRect(0, 5, 10, 10), QRect(100, 0, 10, 10)}));
  QCOMPARE(MonitorArrangement::formatMonitors(monitors), QStringLiteral("0,0,10,10;0,5,10,10;100,0,10,10"));
  QVERIFY(MonitorArrangement::parseMonitors({}).isEmpty());
}

void MonitorArrangementTests::placement_roundTrip()
{
  QPointF position;
  double scale = 0;
  QVERIFY(MonitorArrangement::parsePlacement(MonitorArrangement::formatPlacement({-12.5, 30}, 1.25), position, scale));
  QCOMPARE(position, QPointF(-12.5, 30));
  QCOMPARE(scale, 1.25);
  QVERIFY(!MonitorArrangement::parsePlacement("1,2", position, scale));
  QVERIFY(!MonitorArrangement::parsePlacement("1,2,0", position, scale));
  QVERIFY(!MonitorArrangement::parsePlacement({}, position, scale));
}

QTEST_MAIN(MonitorArrangementTests)
