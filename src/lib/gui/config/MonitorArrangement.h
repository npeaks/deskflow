/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QHash>
#include <QLineF>
#include <QList>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>

/*!
One computer in the monitor arrangement.

Its monitors are in the computer's own desktop coordinates, as the server
reported them, numbered from 1 in canonical order (left edge, then top
edge), which is the numbering the server uses for "@N" links.  The whole
computer is placed on a shared "desk" by putting the top left of its
monitors' bounding box at \c position and scaling it by \c scale, so
computers whose pixels are different physical sizes can be lined up the
way they actually sit.
*/
struct ArrangedComputer
{
  QString name;
  QList<QRect> monitors;
  QPointF position;
  double scale = 1.0;
  bool placed = false;   //!< false until it has a saved or chosen position
  bool reported = false; //!< false if the monitors are a placeholder

  //! Bounding box of the monitors in the computer's own coordinates
  QRect bounds() const;

  //! Monitor \p index (0-based) on the desk
  QRectF deskRect(int index) const;

  //! The whole computer on the desk
  QRectF deskBounds() const;

  bool operator==(const ArrangedComputer &) const = default;
};

/*!
The server's arrangement of every computer's monitors, and the per-monitor
links that follow from it: wherever an edge of one computer's monitor
touches an edge of another computer's monitor on the desk, the cursor can
cross there.
*/
class MonitorArrangement
{
public:
  //! Used when the server hasn't reported a computer's monitors yet
  inline static const QRect kPlaceholderMonitor{0, 0, 1920, 1080};

  //! Edges this close on the desk count as touching
  static constexpr double kTouchTolerance = 2.0;

  //! Edges this close snap together when dragging
  static constexpr double kSnapDistance = 60.0;

  bool enabled() const
  {
    return m_enabled;
  }
  void setEnabled(bool enabled)
  {
    m_enabled = enabled;
  }

  const QList<ArrangedComputer> &computers() const
  {
    return m_computers;
  }
  ArrangedComputer *computer(const QString &name);
  const ArrangedComputer *computer(const QString &name) const;

  /*!
  Make the arrangement contain exactly \p names, keeping what's known about
  computers it already has.  New computers get \p monitors[name] if given
  (else a placeholder) and are left unplaced.
  */
  void setComputers(const QStringList &names, const QHash<QString, QString> &monitors = {});

  //! Replace a computer's monitors with ones the server reported
  void setMonitors(const QString &name, const QList<QRect> &monitors);

  /*!
  Move a computer, snapping its monitor edges to other computers' monitor
  edges within \p snapDistance (0 to not snap).  Returns the new position.
  */
  QPointF moveComputer(const QString &name, QPointF position, double snapDistance = kSnapDistance);

  void setScale(const QString &name, double scale);

  //! Forget every position, so placeUnplaced() lays everything out again
  void resetPlacements();

  /*!
  Place unplaced computers in a row to the right of everything else, in
  \p order (names not in the arrangement are ignored).
  */
  void placeUnplaced(const QStringList &order);

  //! True if any two computers' monitors overlap on the desk
  bool hasOverlaps() const;

  //! Where monitors of different computers touch on the desk, i.e. where the cursor can cross
  QList<QLineF> contacts() const;

  //! The link lines (without indentation) for the computer's section of the server config
  QStringList links(const QString &name) const;

  //! Parse "x,y,w,h;x,y,w,h", sorted into canonical order; invalid entries are dropped
  static QList<QRect> parseMonitors(const QString &text);
  static QString formatMonitors(const QList<QRect> &monitors);

  //! Parse "x,y,scale"; returns false if \p text isn't a placement
  static bool parsePlacement(const QString &text, QPointF &position, double &scale);
  static QString formatPlacement(QPointF position, double scale);

  bool operator==(const MonitorArrangement &) const = default;

private:
  bool m_enabled = false;
  QList<ArrangedComputer> m_computers;
};
