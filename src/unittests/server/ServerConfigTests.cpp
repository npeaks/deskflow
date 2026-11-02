/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigTests.h"

#include "common/Settings.h"
#include "server/Config.h"

#include <QFile>

#include <sstream>

class OnlySystemFilter : public InputFilter::Condition
{
public:
  Condition *clone() const override
  {
    return new OnlySystemFilter();
  }
  std::string format() const override
  {
    return "";
  }

  InputFilter::FilterStatus match(const Event &ev) override
  {
    return ev.getType() == EventTypes::System ? InputFilter::FilterStatus::Activate
                                              : InputFilter::FilterStatus::NoMatch;
  }
};

using namespace deskflow::server;

namespace {
// the links from the per-monitor issue: a portrait (1) and a landscape (2)
// monitor on "win", and a laptop below the landscape monitor
const char *const kMonitorLinks = "section: links\n"
                                  "\twin:\n"
                                  "\t\tdown@2(0,60) = mac@1\n"
                                  "\t\tright@1(56,100) = mac@1\n"
                                  "\tmac:\n"
                                  "\t\tup@1 = win@2(0,60)\n"
                                  "\t\tleft@1 = win@1(56,100)\n"
                                  "\t\tright = win\n"
                                  "end\n";

void readConfig(Config &config, const std::string &text)
{
  std::istringstream stream(text);
  stream >> config;
}
} // namespace

void ServerConfigTests::initTestCase()
{
  m_arch.init();

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists()) {
    oldSettings.remove();
  }
  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);

  // links can only name computers the settings know about
  Settings::setValue(QStringLiteral("computer_win/name"), QStringLiteral("win"));
  Settings::setValue(QStringLiteral("computer_mac/name"), QStringLiteral("mac"));
}

void ServerConfigTests::equalityCheck()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a != b);

  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);

  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.connect("computerB", Direction::Left, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a.addOption(std::string(), kOptionClipboardSharing, 1));
  QVERIFY(b.addOption(std::string(), kOptionClipboardSharing, 1));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  b.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  /* TODO Fix linking to the proper libs
  NetworkAddress addr1("localhost", 8080);
  addr1.resolve();
  NetworkAddress addr2("localhost", 8080);
  addr2.resolve();
  a.setDeskflowAddress(addr1);
  b.setDeskflowAddress(addr2);
  */
  QVERIFY(a == b);
}

void ServerConfigTests::equalityCheck_diff_options()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(a.addOption("computerA", kOptionClipboardSharing, 0));
  QVERIFY(b.addOption("computerA", kOptionClipboardSharing, 1));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_alias()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addAlias("computerA", "aliasA"));
  QVERIFY(a != b);

  QVERIFY(a.addAlias("computerA", "aliasA"));
  QVERIFY(b.addAlias("computerA", "aliasB"));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_filters()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a != b);
}

// TODO FIX
/*
void ServerConfigTests::equalityCheck_diff_address()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(b.addComputer("computerA"));
  a.setDeskflowAddress(NetworkAddress(8000));
  b.setDeskflowAddress(NetworkAddress(9000));
  QVERIFY(a != b);
}
*/

void ServerConfigTests::equalityCheck_diff_neighbours1()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a != b);
  QVERIFY(b != a);
}

void ServerConfigTests::equalityCheck_diff_neighbours2()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.25f, "computerB", 0.25f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_neighbours3()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(a.addComputer("computerC"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.5f, 1.0f));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(b.addComputer("computerC"));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerC", 0.5f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_monitor()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addComputer("computerA"));
  QVERIFY(a.addComputer("computerB"));
  QVERIFY(b.addComputer("computerA"));
  QVERIFY(b.addComputer("computerB"));
  QVERIFY(a.connect("computerA", Direction::Bottom, 0.0f, 1.0f, "computerB", 0.0f, 1.0f, 2, 1));
  QVERIFY(b.connect("computerA", Direction::Bottom, 0.0f, 1.0f, "computerB", 0.0f, 1.0f, 2, 0));
  QVERIFY(a != b);
}

void ServerConfigTests::monitorLinks_connect_sameSideOtherMonitorDoesNotOverlap()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("computerA"));
  QVERIFY(config.addComputer("computerB"));

  // whole computer, monitor 1 and monitor 2 can all have a bottom link
  QVERIFY(config.connect("computerA", Direction::Bottom, 0.0f, 1.0f, "computerB", 0.0f, 1.0f));
  QVERIFY(config.connect("computerA", Direction::Bottom, 0.0f, 1.0f, "computerB", 0.0f, 1.0f, 1));
  QVERIFY(config.connect("computerA", Direction::Bottom, 0.0f, 0.5f, "computerB", 0.0f, 1.0f, 2));

  // but links on the same monitor still can't overlap
  QVERIFY(!config.connect("computerA", Direction::Bottom, 0.25f, 0.75f, "computerB", 0.0f, 1.0f, 2));
  QVERIFY(config.connect("computerA", Direction::Bottom, 0.5f, 1.0f, "computerB", 0.0f, 1.0f, 2));
}

void ServerConfigTests::monitorLinks_getMonitorNeighbor()
{
  Config config(nullptr);
  QVERIFY(config.addComputer("win"));
  QVERIFY(config.addComputer("mac"));
  QVERIFY(config.connect("win", Direction::Bottom, 0.0f, 0.5f, "mac", 0.0f, 1.0f, 2, 1));

  // halfway along the linked half of monitor 2 lands halfway along the mac
  const auto target = config.getMonitorNeighbor("win", 2, Direction::Bottom, 0.25f);
  QVERIFY(target.has_value());
  QCOMPARE(target->m_name, std::string("mac"));
  QCOMPARE(target->m_monitor, 1);
  QVERIFY(target->m_position > 0.49f && target->m_position < 0.51f);

  // outside the interval, other monitors and the whole computer have no link
  QVERIFY(!config.getMonitorNeighbor("win", 2, Direction::Bottom, 0.75f).has_value());
  QVERIFY(!config.getMonitorNeighbor("win", 1, Direction::Bottom, 0.25f).has_value());
  QVERIFY(!config.getMonitorNeighbor("win", 2, Direction::Top, 0.25f).has_value());
  QVERIFY(config.getNeighbor("win", Direction::Bottom, 0.25f, nullptr).empty());

  QVERIFY(config.hasMonitorNeighbor("win", Direction::Bottom));
  QVERIFY(!config.hasMonitorNeighbor("win", Direction::Right));
  QVERIFY(!config.hasNeighbor("win", Direction::Bottom));
}

void ServerConfigTests::monitorLinks_parse()
{
  Config config(nullptr);
  readConfig(config, kMonitorLinks);

  auto target = config.getMonitorNeighbor("win", 2, Direction::Bottom, 0.3f);
  QVERIFY(target.has_value());
  QCOMPARE(target->m_name, std::string("mac"));
  QCOMPARE(target->m_monitor, 1);
  QVERIFY(target->m_position > 0.49f && target->m_position < 0.51f);

  target = config.getMonitorNeighbor("win", 1, Direction::Right, 0.9f);
  QVERIFY(target.has_value());
  QCOMPARE(target->m_monitor, 1);

  target = config.getMonitorNeighbor("mac", 1, Direction::Top, 0.5f);
  QVERIFY(target.has_value());
  QCOMPARE(target->m_name, std::string("win"));
  QCOMPARE(target->m_monitor, 2);
  QVERIFY(target->m_position > 0.29f && target->m_position < 0.31f);

  // plain links still work alongside monitor links
  QCOMPARE(config.getNeighbor("mac", Direction::Right, 0.5f, nullptr), std::string("win"));
}

void ServerConfigTests::monitorLinks_writeThenRead_roundTrips()
{
  Config original(nullptr);
  readConfig(original, kMonitorLinks);

  std::ostringstream written;
  written << original;
  QVERIFY(written.str().find("down@2(0,60) = mac@1") != std::string::npos);
  QVERIFY(written.str().find("up@1 = win@2(0,60)") != std::string::npos);

  // compare the links sections by writing again; Config::operator== and the
  // options section also involve network addresses, which aren't set up for
  // configs read from a stream
  Config reread(nullptr);
  readConfig(reread, written.str());
  std::ostringstream rewritten;
  rewritten << reread;
  const auto linksOf = [](const std::string &text) { return text.substr(0, text.find("end\n")); };
  QCOMPARE(linksOf(rewritten.str()), linksOf(written.str()));
}

void ServerConfigTests::monitorLinks_parse_invalidMonitor_throws_data()
{
  QTest::addColumn<QString>("link");
  QTest::newRow("zero") << QStringLiteral("down@0 = mac");
  QTest::newRow("empty") << QStringLiteral("down@ = mac");
  QTest::newRow("not a number") << QStringLiteral("down@two = mac");
  QTest::newRow("trailing junk") << QStringLiteral("down@2x = mac");
  QTest::newRow("destination") << QStringLiteral("down = mac@-1");
}

void ServerConfigTests::monitorLinks_parse_invalidMonitor_throws()
{
  QFETCH(QString, link);
  const std::string text = "section: links\n\twin:\n\t\t" + link.toStdString() + "\nend\n";

  Config config(nullptr);
  bool threw = false;
  try {
    readConfig(config, text);
  } catch (const ServerConfigReadException &) {
    threw = true;
  }
  QVERIFY(threw);
}

QTEST_MAIN(ServerConfigTests)
