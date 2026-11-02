/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QPointF>
#include <QString>
#include <QTransform>
#include <QWidget>

class MonitorArrangement;

/*!
Shows every computer's monitors on a shared desk and lets the user drag
computers into place, like the display arrangement in OS settings.  Edges
where two computers touch are highlighted, since that's where the cursor
will cross.
*/
class MonitorArrangementView : public QWidget
{
  Q_OBJECT

public:
  explicit MonitorArrangementView(QWidget *parent = nullptr);

  //! The arrangement to show and edit; not owned
  void setArrangement(MonitorArrangement *arrangement);

  QString selectedComputer() const
  {
    return m_selected;
  }
  void setSelectedComputer(const QString &name);

  //! Recompute the zoom to fit everything, then repaint
  void refit();

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

Q_SIGNALS:
  //! The user moved a computer
  void arrangementChanged();
  void selectionChanged(const QString &name);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QString computerAt(QPointF widgetPos) const;
  void updateTransform();

  MonitorArrangement *m_arrangement = nullptr;
  QTransform m_toWidget; //!< desk coordinates to widget coordinates
  QString m_selected;
  QString m_dragging;
  QPointF m_dragOffset; //!< desk offset from the grab point to the computer's position
  bool m_moved = false;
};
