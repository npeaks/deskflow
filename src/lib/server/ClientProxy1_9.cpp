/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>
#include <vector>

namespace {
// more than any real desk; protects against nonsense from a broken client
constexpr size_t kMaxMonitors = 64;
} // namespace

ClientProxy1_9::ClientProxy1_9(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events)
{
}

deskflow::MonitorList ClientProxy1_9::getMonitors() const
{
  // only trust the monitors if they fit the shape the client last sent,
  // otherwise behave like a pre-1.9 client.
  if (!m_monitors.empty()) {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    getShape(x, y, w, h);
    const auto b = deskflow::monitors::bounds(m_monitors);
    if (b.x >= x && b.y >= y && b.x + b.w <= x + w && b.y + b.h <= y + h) {
      return m_monitors;
    }
  }
  return ClientProxy1_8::getMonitors();
}

bool ClientProxy1_9::parseHandshakeMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDMonitors, 4) == 0) {
    return recvMonitors();
  }
  return ClientProxy1_8::parseHandshakeMessage(code);
}

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDMonitors, 4) == 0) {
    return recvMonitors();
  }
  return ClientProxy1_8::parseMessage(code);
}

bool ClientProxy1_9::recvMonitors()
{
  std::vector<uint32_t> values;
  if (!ProtocolUtil::readf(getStream(), kMsgDMonitors + 4, &values)) {
    return false;
  }

  if (values.size() % 4 != 0 || values.size() / 4 > kMaxMonitors) {
    LOG_WARN("ignoring malformed monitor list from \"%s\" (%zu values)", getName().c_str(), values.size());
    m_monitors.clear();
    return true;
  }

  deskflow::MonitorList monitors;
  for (size_t i = 0; i < values.size(); i += 4) {
    monitors.push_back(
        {static_cast<int32_t>(values[i]), static_cast<int32_t>(values[i + 1]), static_cast<int32_t>(values[i + 2]),
         static_cast<int32_t>(values[i + 3])}
    );
  }
  deskflow::monitors::normalize(monitors);
  m_monitors = std::move(monitors);

  LOG_DEBUG("received client \"%s\" monitors: %s", getName().c_str(), deskflow::monitors::toString(m_monitors).c_str());
  return true;
}
