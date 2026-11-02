/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_9.h"

#include "base/Log.h"
#include "client/Client.h"
#include "deskflow/MonitorLayout.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <vector>

ServerProxy1_9::ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_8(client, stream, events)
{
}

void ServerProxy1_9::sendMonitors()
{
  const auto monitors = getClient()->getMonitors();

  std::vector<uint32_t> values;
  values.reserve(monitors.size() * 4);
  for (const auto &m : monitors) {
    values.push_back(static_cast<uint32_t>(m.x));
    values.push_back(static_cast<uint32_t>(m.y));
    values.push_back(static_cast<uint32_t>(m.w));
    values.push_back(static_cast<uint32_t>(m.h));
  }

  LOG_VERBOSE("sending monitors: %s", deskflow::monitors::toString(monitors).c_str());
  ProtocolUtil::writef(getStream(), kMsgDMonitors, &values);
}
