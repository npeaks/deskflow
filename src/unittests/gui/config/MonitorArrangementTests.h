/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class MonitorArrangementTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void links_desk();
  void links_gap_none();
  void links_sameComputer_none();
  void links_overlappingMonitors_intervalsDontOverlap();
  void links_scaled();
  void contacts_desk();
  void moveComputer_snapsToNearbyEdge();
  void moveComputer_noSnap();
  void hasOverlaps();
  void setComputers_keepsExisting_placeholderForUnknown();
  void placeUnplaced_rowTouching();
  void parseMonitors_sortsAndDropsInvalid();
  void placement_roundTrip();
};
