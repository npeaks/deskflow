/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_8.h"

//! Proxy for server implementing protocol version 1.9
/*!
1.9 adds kMsgDMonitors: the client tells the server about each of its
monitors so the server can switch on real monitor edges.
*/
class ServerProxy1_9 : public ServerProxy1_8
{
public:
  ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events);
  ~ServerProxy1_9() override = default;

protected:
  void sendMonitors() override;
};
