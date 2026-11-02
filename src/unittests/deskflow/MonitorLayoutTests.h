/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class MonitorLayoutTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void normalize_sortsByLeftThenTop_dropsInvalid();
  void bounds_enclosesAll();
  void find_pointOnMonitor();
  void find_pointInGap_none();
  void nearest_pointInGap_closestMonitor();
  void isExposed_sharedEdge_false();
  void isExposed_openEdge_true();
  void isOnBoundsEdge_innerEdge_false();
  void fraction_roundTrip();
  void fromFraction_clampsOntoMonitor();
  void toString_numbersFromOne();
};
