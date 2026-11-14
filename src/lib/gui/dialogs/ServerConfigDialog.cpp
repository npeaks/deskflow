/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialog.h"
#include "ui_ServerConfigDialog.h"

#include "common/Constants.h"
#include "common/NetworkProtocol.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "dialogs/ActionDialog.h"
#include "dialogs/ComputerSettingsDialog.h"
#include "dialogs/HotkeyDialog.h"
#include "gui/widgets/MonitorArrangementView.h"
#include "gui/widgets/SettingsDialogButtonBox.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using enum ComputerConfig::SwitchCorner;

ServerConfigDialog::ServerConfigDialog(QWidget *parent, ServerConfig &config)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ServerConfigDialog>()},
      m_columns{Settings::value(Settings::Server::GridWidth).toInt()},
      m_rows{Settings::value(Settings::Server::GridHeight).toInt()},
      m_originalServerConfig(config),
      m_originalServerConfigIsExternal(config.useExternalConfig()),
      m_originalServerConfigUsesExternalFile(config.configFile()),
      m_serverConfig(config),
      m_computerSetupModel(m_serverConfig.computers(), m_columns, m_rows),
      m_buttonBox{new SettingsDialogButtonBox(this)}
{
  ui->setupUi(this);
  setupLayoutTab();
  ui->tabWidget->setCurrentIndex(0);
  layout()->addWidget(m_buttonBox);

  loadFromConfig();

  ui->btnBrowseConfigFile->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));

  if (!deskflow::platform::isWindows())
    ui->cbWin32KeepForeground->setVisible(false);
  initConnections();
}

ServerConfigDialog::~ServerConfigDialog() = default;

bool ServerConfigDialog::addClient(const QString &clientName)
{
  return addComputer(clientName, true);
}

void ServerConfigDialog::save()
{
  if (ui->groupExternalConfig->isChecked() && !QFile::exists(ui->lineConfigFile->text())) {

    auto selectedButton = QMessageBox::warning(
        this, "Filename invalid", "Please select a valid configuration file.", QMessageBox::Ok | QMessageBox::Ignore
    );

    if (selectedButton != QMessageBox::Ok || !browseConfigFile()) {
      return;
    }
  }
  // now that the dialog has been accepted, copy the new server config to the
  // original one, which is a reference to the one in MainWindow.
  setOriginalServerConfig(serverConfig());
  if (Settings::value(Settings::Server::EnableClipboard).toBool() != m_enableClipboard) {
    Settings::setValue(Settings::Server::XdpRestoreToken, QString());
    Settings::setValue(Settings::Server::XdpClipboardRetried, false);
  }
  Settings::setValue(Settings::Server::Protocol, networkProtocolToOption(m_protocol));
  Settings::setValue(Settings::Server::EnableClipboard, m_enableClipboard);
  Settings::setValue(Settings::Server::ClipboardSize, m_clipboardSize);
  Settings::setValue(Settings::Server::EnableHeartbeat, m_enableHeartbeat);
  Settings::setValue(Settings::Server::Heartbeat, m_heartbeatRate);
  Settings::setValue(Settings::Server::EnableSwitchDelay, m_enableSwitchDelay);
  Settings::setValue(Settings::Server::SwitchDelay, m_switchDelay);
  Settings::setValue(Settings::Server::DefaultLockToComputerState, m_defaultLockToComputerState);
  Settings::setValue(Settings::Server::DisableLockToComputer, m_disableLockToComputer);
  Settings::setValue(Settings::Server::EnableSwitchDoubleTap, m_enableSwitchDoubleTap);
  Settings::setValue(Settings::Server::SwitchDoubleTap, m_switchDoubleTap);
  Settings::setValue(Settings::Server::RelativeMouseMoves, m_relativeMouseMoves);
  Settings::setValue(Settings::Server::Win32KeepForeground, m_win32keepForeground);
  Settings::setValue(Settings::Server::ExternalConfig, ui->groupExternalConfig->isChecked());
  Settings::setValue(Settings::Server::ExternalConfigFile, ui->lineConfigFile->text());

  QStringList computerNames;
  const auto computerList = m_computerSetupModel.m_computers;
  for (const auto &computer : computerList) {
    const auto &computerName = computer.name();
    if (computerName.isEmpty())
      continue;
    computerNames.append(QStringLiteral("computer_%1").arg(computerName));
    Settings::setValue(Settings::Computer::Aliases.arg(computerName), computer.aliases());
    Settings::setValue(Settings::Computer::Name.arg(computerName), computerName);
  }
  // Write the computer list now too, or it keeps computers whose settings were just removed until the app quits.
  m_serverConfig.commit();
  Settings::removeUnknownComputers(computerNames);
  QDialog::accept();
}

void ServerConfigDialog::cancel()
{
  serverConfig().setUseExternalConfig(m_originalServerConfigIsExternal);
  serverConfig().setConfigFile(m_originalServerConfigUsesExternalFile);
  QDialog::reject();
}

void ServerConfigDialog::addHotkey()
{
  Hotkey hotkey;
  HotkeyDialog dlg(this, hotkey);
  if (dlg.exec() == QDialog::Accepted) {
    serverConfig().hotkeys().append(hotkey);
    ui->listHotkeys->addItem(hotkey.text());
    setButtonBoxEnabledButtons();
  }
}

void ServerConfigDialog::editHotkey()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to editing out of bounds hotkey row: " << row;
    return;
  }

  Hotkey &hotkey = serverConfig().hotkeys()[row];
  HotkeyDialog dlg(this, hotkey);
  if (dlg.exec() == QDialog::Accepted) {
    ui->listHotkeys->currentItem()->setText(hotkey.text());
    setButtonBoxEnabledButtons();
  }
}

void ServerConfigDialog::removeHotkey()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to remove out of bounds hotkey row: " << row;
    return;
  }

  serverConfig().hotkeys().removeAt(row);
  ui->listActions->clear();
  delete ui->listHotkeys->item(row);
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::listHotkeysSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
  bool itemsSelected = !selected.isEmpty();
  ui->btnEditHotkey->setEnabled(itemsSelected);
  ui->btnRemoveHotkey->setEnabled(itemsSelected);
  ui->btnNewAction->setEnabled(itemsSelected);

  if (itemsSelected && !serverConfig().hotkeys().isEmpty()) {
    ui->listActions->clear();
    const Hotkey &hotkey = serverConfig().hotkeys().at(selected.indexes().first().row());
    for (const Action &action : hotkey.actions())
      ui->listActions->addItem(action.text());
  }
}

void ServerConfigDialog::addAction()
{
  int row = ui->listHotkeys->currentRow();
  if (row < 0 || row >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to add action to out of bounds hotkey row: " << row;
    return;
  }

  Hotkey &hotkey = serverConfig().hotkeys()[row];
  Action action;
  ActionDialog dlg(this, serverConfig(), hotkey, action);
  if (dlg.exec() == QDialog::Accepted) {
    hotkey.addAction(action);
    ui->listActions->addItem(action.text());
    setButtonBoxEnabledButtons();
  }
}

void ServerConfigDialog::editAction()
{
  int hotkeyRow = ui->listHotkeys->currentRow();
  if (hotkeyRow < 0 || hotkeyRow >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to edit action from out of bounds hotkey row: " << hotkeyRow;
    return;
  }
  Hotkey &hotkey = serverConfig().hotkeys()[hotkeyRow];

  int actionRow = ui->listActions->currentRow();
  if (actionRow < 0 || actionRow >= hotkey.actions().size()) {
    qDebug() << "Attempt to remove out of bounds action row: " << actionRow;
    return;
  }
  Action &action = hotkey.actionAt(actionRow);

  ActionDialog dlg(this, serverConfig(), hotkey, action);
  if (dlg.exec() == QDialog::Accepted) {
    ui->listActions->currentItem()->setText(action.text());
    setButtonBoxEnabledButtons();
  }
}

void ServerConfigDialog::removeAction()
{
  int hotkeyRow = ui->listHotkeys->currentRow();
  if (hotkeyRow < 0 || hotkeyRow >= serverConfig().hotkeys().size()) {
    qDebug() << "Attempt to remove action from out of bounds hotkey row: " << hotkeyRow;
    return;
  }
  Hotkey &hotkey = serverConfig().hotkeys()[hotkeyRow];

  int actionRow = ui->listActions->currentRow();
  if (actionRow < 0 || actionRow >= hotkey.actions().size()) {
    qDebug() << "Attempt to remove out of bounds action row: " << actionRow;
    return;
  }

  hotkey.removeActionAt(actionRow);
  delete ui->listActions->currentItem();
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleClipboard(bool enabled)
{
  if (m_enableClipboard == enabled)
    return;

  m_enableClipboard = enabled;

  ui->sbClipboardSizeLimit->setEnabled(enabled);
  if (enabled && !ui->sbClipboardSizeLimit->value()) {
    m_clipboardSize = Settings::defaultValue(Settings::Server::ClipboardSize).toUInt();
    ui->sbClipboardSizeLimit->setValue(m_clipboardSize ? m_clipboardSize : 1);
  }
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::setClipboardLimit(int limit)
{
  if (m_clipboardSize == limit)
    return;

  m_clipboardSize = limit;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleHeartbeat(bool enabled)
{
  m_enableHeartbeat = enabled;
  ui->sbHeartbeat->setEnabled(enabled);
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::setHeartbeat(int rate)
{
  if (rate == m_heartbeatRate)
    return;
  m_heartbeatRate = rate;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleRelativeMouseMoves(bool enabled)
{
  if (m_relativeMouseMoves == enabled)
    return;
  m_relativeMouseMoves = enabled;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleProtocol()
{
  m_protocol = ui->rbProtocolBarrier->isChecked() ? NetworkProtocol::Barrier : NetworkProtocol::Synergy;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::listActionsSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
  bool enabled = !selected.isEmpty();
  ui->btnEditAction->setEnabled(enabled);
  ui->btnRemoveAction->setEnabled(enabled);
}

void ServerConfigDialog::toggleSwitchDoubleTap(bool enable)
{
  m_enableSwitchDoubleTap = enable;
  ui->sbSwitchDoubleTap->setEnabled(enable);
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::setSwitchDoubleTap(int within)
{
  if (m_switchDoubleTap == within)
    return;
  m_switchDoubleTap = within;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleSwitchDelay(bool enable)
{
  m_enableSwitchDelay = enable;
  ui->sbSwitchDelay->setEnabled(enable);
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::setSwitchDelay(int delay)
{
  if (m_switchDelay == delay)
    return;
  m_switchDelay = delay;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleDefaultLockToComputerState(bool state)
{
  if (m_defaultLockToComputerState == state)
    return;
  m_defaultLockToComputerState = state;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleLockToComputer(bool disabled)
{
  if (m_disableLockToComputer == disabled)
    return;
  m_disableLockToComputer = disabled;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::toggleWin32Foreground(bool enabled)
{
  if (m_win32keepForeground == enabled)
    return;
  m_win32keepForeground = enabled;
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::addClient()
{
  addComputer("", false);
}

void ServerConfigDialog::toggleExternalConfig(bool checked)
{
  ui->widgetExternalConfigControls->setEnabled(checked);
  ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->tabComputers), !checked);
  ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->tabHotkeys), !checked);
  serverConfig().setUseExternalConfig(checked);
  setButtonBoxEnabledButtons();
}

bool ServerConfigDialog::browseConfigFile()
{
  //: %1 is replaced with the application names
  //: (*.conf) and (*.*) should not be translated
  const auto deskflowConfigFilter = tr("%1 Configurations (*.conf);;All files (*.*)");

  QString fileName =
      QFileDialog::getOpenFileName(this, tr("Browse for a config file"), "", deskflowConfigFilter.arg(kAppName));

  if (!fileName.isEmpty()) {
    ui->lineConfigFile->setText(fileName);
    setServerConfig();
    return true;
  }

  return false;
}

void ServerConfigDialog::loadFromConfig()
{
  m_protocol = Settings::networkProtocol();
  m_enableHeartbeat = Settings::value(Settings::Server::EnableHeartbeat).toBool();
  m_heartbeatRate = Settings::value(Settings::Server::Heartbeat).toInt();
  m_relativeMouseMoves = Settings::value(Settings::Server::RelativeMouseMoves).toBool();
  m_win32keepForeground = Settings::value(Settings::Server::Win32KeepForeground).toBool();
  m_enableSwitchDelay = Settings::value(Settings::Server::EnableSwitchDelay).toBool();
  m_switchDelay = Settings::value(Settings::Server::SwitchDelay).toInt();
  m_enableSwitchDoubleTap = Settings::value(Settings::Server::EnableSwitchDoubleTap).toBool();
  m_switchDoubleTap = Settings::value(Settings::Server::SwitchDoubleTap).toInt();
  m_defaultLockToComputerState = Settings::value(Settings::Server::DefaultLockToComputerState).toBool();
  m_disableLockToComputer = Settings::value(Settings::Server::DisableLockToComputer).toBool();
  m_enableClipboard = Settings::value(Settings::Server::EnableClipboard).toBool();
  m_clipboardSize = Settings::value(Settings::Server::ClipboardSize).toUInt();

  ui->lineConfigFile->setText(serverConfig().configFile());
  ui->groupExternalConfig->setChecked(serverConfig().useExternalConfig());

  refreshControls();

  ui->listHotkeys->clear();
  for (const Hotkey &hotkey : std::as_const(serverConfig().hotkeys()))
    ui->listHotkeys->addItem(hotkey.text());

  auto &computers = serverConfig().computers();
  auto server = std::ranges::find_if(computers, [this](const Computer &computer) {
    return (computer.name() == serverConfig().getServerName());
  });

  if (server == computers.end()) {
    Computer serverComputer(serverConfig().getServerName());
    serverComputer.markAsServer();
    model().computer(m_columns / 2, m_rows / 2) = serverComputer;
  } else {
    server->markAsServer();
  }
  refreshLayoutTab();
  updateControls();
}

void ServerConfigDialog::setupLayoutTab()
{
  auto *tab = ui->tabComputers;
  auto *layout = qobject_cast<QVBoxLayout *>(tab->layout());

  auto *help = new QLabel(
      tr("Drag computers so their monitors sit the way they do on your desk. The cursor moves between computers "
         "wherever their monitors touch (highlighted). A computer shows its real monitors once it has connected to "
         "this server. Hold Shift while dragging to turn off snapping. Double click a computer to change its "
         "settings."),
      tab
  );
  help->setWordWrap(true);
  layout->addWidget(help);

  auto *body = new QHBoxLayout();
  layout->addLayout(body, 1);

  // the computers, and the settings of the selected one
  auto *side = new QVBoxLayout();
  body->addLayout(side);

  m_listComputers = new QListWidget(tab);
  m_listComputers->setMaximumWidth(240);
  side->addWidget(m_listComputers, 1);

  auto *listButtons = new QHBoxLayout();
  m_btnAddComputer = new QPushButton(tr("&Add"), tab);
  m_btnAddComputer->setToolTip(tr("Add a computer that will connect to this one"));
  m_btnEditComputer = new QPushButton(tr("&Edit"), tab);
  m_btnEditComputer->setToolTip(tr("Change the name, aliases, keys and other settings of the selected computer"));
  m_btnRemoveComputer = new QPushButton(tr("&Remove"), tab);
  listButtons->addWidget(m_btnAddComputer);
  listButtons->addWidget(m_btnEditComputer);
  listButtons->addWidget(m_btnRemoveComputer);
  side->addLayout(listButtons);

  m_lblMonitorScale = new QLabel(tab);
  m_lblMonitorScale->setWordWrap(true);
  side->addWidget(m_lblMonitorScale);
  m_sbMonitorScale = new QDoubleSpinBox(tab);
  m_sbMonitorScale->setRange(0.1, 10.0);
  m_sbMonitorScale->setSingleStep(0.05);
  m_sbMonitorScale->setDecimals(2);
  m_sbMonitorScale->setSuffix(QStringLiteral("×"));
  m_sbMonitorScale->setToolTip(
      tr("Make this computer's monitors bigger or smaller on the desk, to match their real size next to the others")
  );
  side->addWidget(m_sbMonitorScale);

  // the desk
  auto *deskSide = new QVBoxLayout();
  body->addLayout(deskSide, 1);
  m_monitorView = new MonitorArrangementView(tab);
  deskSide->addWidget(m_monitorView, 1);

  auto *deskControls = new QHBoxLayout();
  m_lblMonitorSummary = new QLabel(tab);
  deskControls->addWidget(m_lblMonitorSummary);
  deskControls->addStretch();
  m_btnRearrangeMonitors = new QPushButton(tr("Rearrange"), tab);
  m_btnRearrangeMonitors->setToolTip(tr("Put every computer back in a row, in list order"));
  deskControls->addWidget(m_btnRearrangeMonitors);
  deskSide->addLayout(deskControls);

  connect(m_btnAddComputer, &QPushButton::clicked, this, [this] { addComputer({}, false); });
  connect(m_btnEditComputer, &QPushButton::clicked, this, [this] { editComputer(m_monitorView->selectedComputer()); });
  connect(m_btnRemoveComputer, &QPushButton::clicked, this, &ServerConfigDialog::removeSelectedComputer);
  connect(m_listComputers, &QListWidget::currentItemChanged, this, [this](const QListWidgetItem *item) {
    m_monitorView->setSelectedComputer(item != nullptr ? item->data(Qt::UserRole).toString() : QString());
  });
  connect(m_listComputers, &QListWidget::itemActivated, this, [this](const QListWidgetItem *item) {
    editComputer(item->data(Qt::UserRole).toString());
  });
  connect(
      m_sbMonitorScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ServerConfigDialog::setMonitorScale
  );
  connect(m_btnRearrangeMonitors, &QPushButton::clicked, this, &ServerConfigDialog::rearrangeMonitors);
  connect(m_monitorView, &MonitorArrangementView::selectionChanged, this, &ServerConfigDialog::monitorSelectionChanged);
  connect(m_monitorView, &MonitorArrangementView::computerActivated, this, &ServerConfigDialog::editComputer);
  connect(
      m_monitorView, &MonitorArrangementView::arrangementChanged, this, &ServerConfigDialog::monitorArrangementChanged
  );
  connect(&m_computerSetupModel, &ComputerSetupModel::computersChanged, this, &ServerConfigDialog::refreshLayoutTab);
  connect(Settings::instance(), &Settings::settingsChanged, this, [this](const QString &key) {
    // a computer connected (or changed its displays) while the dialog is open
    if (key.startsWith(QStringLiteral("computer_")) && key.endsWith(QStringLiteral("/monitors"))) {
      refreshLayoutTab();
    }
  });
}

void ServerConfigDialog::refreshLayoutTab()
{
  if (m_monitorView == nullptr) {
    return;
  }
  auto &arrangement = serverConfig().arrangement();
  serverConfig().syncArrangement();

  m_monitorView->setArrangement(&arrangement);
  if (arrangement.computer(m_monitorView->selectedComputer()) == nullptr) {
    m_monitorView->setSelectedComputer({});
  }
  refreshComputerList();
  monitorSelectionChanged(m_monitorView->selectedComputer());
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::refreshComputerList()
{
  const QSignalBlocker blocker(m_listComputers);
  m_listComputers->clear();
  for (const auto &computer : std::as_const(model().m_computers)) {
    if (computer.isNull()) {
      continue;
    }
    auto *item = new QListWidgetItem(
        computer.isServer() ? tr("%1 (this computer)").arg(computer.name()) : computer.name(), m_listComputers
    );
    item->setData(Qt::UserRole, computer.name());
    if (computer.name() == m_monitorView->selectedComputer()) {
      m_listComputers->setCurrentItem(item);
    }
  }
}

Computer *ServerConfigDialog::findComputer(const QString &name)
{
  if (name.isEmpty()) {
    return nullptr;
  }
  auto &computers = model().m_computers;
  auto it = std::ranges::find_if(computers, [&name](const Computer &c) { return c.name() == name; });
  return it == computers.end() ? nullptr : &*it;
}

void ServerConfigDialog::editComputer(const QString &name)
{
  auto *computer = findComputer(name);
  if (computer == nullptr) {
    return;
  }
  if (ComputerSettingsDialog dlg(this, computer, &model().m_computers); dlg.exec() != QDialog::Accepted) {
    return;
  }

  if (const auto &newName = computer->name(); newName != name) {
    // keep its place on the desk, and the monitors it last reported
    serverConfig().arrangement().renameComputer(name, newName);
    const auto monitors = Settings::value(Settings::Computer::Monitors.arg(name)).toString();
    if (!monitors.isEmpty() && Settings::value(Settings::Computer::Monitors.arg(newName)).toString().isEmpty()) {
      Settings::setValue(Settings::Computer::Monitors.arg(newName), monitors);
    }
    m_monitorView->setSelectedComputer(newName);
  }
  Q_EMIT model().computersChanged();
}

void ServerConfigDialog::removeSelectedComputer()
{
  auto *computer = findComputer(m_monitorView->selectedComputer());
  if (computer == nullptr || computer->isServer()) {
    return;
  }
  *computer = Computer();
  m_monitorView->setSelectedComputer({});
  Q_EMIT model().computersChanged();
}

void ServerConfigDialog::updateMonitorSummary() const
{
  const auto &arrangement = m_serverConfig.arrangement();
  if (arrangement.hasOverlaps()) {
    m_lblMonitorSummary->setText(tr("Monitors overlap."));
  } else {
    const auto crossings = arrangement.contacts().size();
    m_lblMonitorSummary->setText(
        crossings == 0   ? tr("No computers touch yet.")
        : crossings == 1 ? tr("The cursor can cross at 1 place.")
                         : tr("The cursor can cross at %1 places.").arg(crossings)
    );
  }
}

void ServerConfigDialog::setMonitorScale(double scale)
{
  const auto name = m_monitorView->selectedComputer();
  if (name.isEmpty()) {
    return;
  }
  serverConfig().arrangement().setScale(name, scale);
  m_monitorView->refit();
  monitorArrangementChanged();
}

void ServerConfigDialog::rearrangeMonitors()
{
  serverConfig().arrangement().resetPlacements();
  serverConfig().arrangement().placeUnplaced({});
  m_monitorView->refit();
  monitorArrangementChanged();
}

void ServerConfigDialog::monitorSelectionChanged(const QString &name)
{
  const auto *computer = serverConfig().arrangement().computer(name);
  const auto *config = findComputer(name);

  // keep the list in step with clicks on the desk
  {
    const QSignalBlocker blocker(m_listComputers);
    m_listComputers->setCurrentItem(nullptr);
    for (int i = 0; i < m_listComputers->count(); ++i) {
      if (m_listComputers->item(i)->data(Qt::UserRole).toString() == name) {
        m_listComputers->setCurrentRow(i);
        break;
      }
    }
  }

  m_btnAddComputer->setEnabled(!model().isFull());
  m_btnEditComputer->setEnabled(config != nullptr);
  m_btnRemoveComputer->setEnabled(config != nullptr && !config->isServer());

  m_lblMonitorScale->setText(
      computer != nullptr ? tr("Size of %1 on the desk:").arg(name) : tr("Select a computer to change it.")
  );
  m_sbMonitorScale->setVisible(computer != nullptr);
  if (computer != nullptr) {
    const QSignalBlocker blocker(m_sbMonitorScale);
    m_sbMonitorScale->setValue(computer->scale);
  }
  updateMonitorSummary();
}

void ServerConfigDialog::monitorArrangementChanged()
{
  updateMonitorSummary();
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::resetFromSettings()
{
  m_serverConfig = m_originalServerConfig;
  m_serverConfig.setConfigFile(m_originalServerConfigUsesExternalFile);
  m_serverConfig.setUseExternalConfig(m_originalServerConfigIsExternal);
  loadFromConfig();
}

void ServerConfigDialog::refreshControls()
{
  ui->rbProtocolSynergy->setChecked(m_protocol == NetworkProtocol::Synergy);
  ui->rbProtocolBarrier->setChecked(m_protocol == NetworkProtocol::Barrier);
  ui->cbHeartbeat->setChecked(m_enableHeartbeat);
  ui->sbHeartbeat->setEnabled(ui->cbHeartbeat->isChecked());
  ui->sbHeartbeat->setValue(m_heartbeatRate);
  ui->cbRelativeMouseMoves->setChecked(m_relativeMouseMoves);
  ui->cbWin32KeepForeground->setChecked(m_win32keepForeground);
  ui->cbSwitchDelay->setChecked(m_enableSwitchDelay);
  ui->sbSwitchDelay->setEnabled(ui->cbSwitchDelay->isChecked());
  ui->sbSwitchDelay->setValue(m_switchDelay);
  ui->cbSwitchDoubleTap->setChecked(m_enableSwitchDoubleTap);
  ui->sbSwitchDoubleTap->setEnabled(ui->cbSwitchDoubleTap->isChecked());
  ui->sbSwitchDoubleTap->setValue(m_switchDoubleTap);
  ui->widgetExternalConfigControls->setEnabled(ui->groupExternalConfig->isChecked());
  toggleExternalConfig(ui->groupExternalConfig->isChecked());
  ui->cbDefaultLockToComputerState->setChecked(m_defaultLockToComputerState);
  ui->cbDisableLockToComputer->setChecked(m_disableLockToComputer);
  ui->cbEnableClipboard->setChecked(m_enableClipboard);
  ui->sbClipboardSizeLimit->setEnabled(m_enableClipboard);
  ui->sbClipboardSizeLimit->setValue(m_clipboardSize);
}

void ServerConfigDialog::initConnections() const
{
  connect(m_buttonBox, &SettingsDialogButtonBox::accepted, this, &ServerConfigDialog::save);
  connect(m_buttonBox, &SettingsDialogButtonBox::rejected, this, &ServerConfigDialog::cancel);
  connect(m_buttonBox, &SettingsDialogButtonBox::reset, this, &ServerConfigDialog::resetFromSettings);
  connect(m_buttonBox, &SettingsDialogButtonBox::restoreDefault, this, &ServerConfigDialog::restoreFromDefaults);
  connect(ui->tabWidget, &QTabWidget::currentChanged, this, &ServerConfigDialog::setButtonBoxEnabledButtons);
  connect(ui->btnNewHotkey, &QPushButton::clicked, this, &ServerConfigDialog::addHotkey);
  connect(ui->btnEditHotkey, &QPushButton::clicked, this, &ServerConfigDialog::editHotkey);
  connect(ui->btnRemoveHotkey, &QPushButton::clicked, this, &ServerConfigDialog::removeHotkey);
  connect(ui->listHotkeys, &QListView::doubleClicked, this, &ServerConfigDialog::editHotkey);
  connect(
      ui->listHotkeys->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      &ServerConfigDialog::listHotkeysSelectionChanged
  );

  connect(ui->btnNewAction, &QPushButton::clicked, this, &ServerConfigDialog::addAction);
  connect(ui->btnEditAction, &QPushButton::clicked, this, &ServerConfigDialog::editAction);
  connect(ui->btnRemoveAction, &QPushButton::clicked, this, &ServerConfigDialog::removeAction);
  connect(ui->listActions, &QListView::doubleClicked, this, &ServerConfigDialog::editAction);
  connect(
      ui->listActions->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      &ServerConfigDialog::listActionsSelectionChanged
  );

  connect(ui->rbProtocolBarrier, &QRadioButton::toggled, this, &ServerConfigDialog::toggleProtocol);
  connect(ui->cbHeartbeat, &QCheckBox::toggled, this, &ServerConfigDialog::toggleHeartbeat);
  connect(ui->sbHeartbeat, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setHeartbeat);
  connect(ui->cbWin32KeepForeground, &QCheckBox::toggled, this, &ServerConfigDialog::toggleWin32Foreground);
  connect(ui->cbSwitchDelay, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDelay);
  connect(ui->sbSwitchDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDelay);
  connect(ui->cbSwitchDoubleTap, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDoubleTap);
  connect(
      ui->sbSwitchDoubleTap, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDoubleTap
  );

  connect(ui->cbRelativeMouseMoves, &QCheckBox::toggled, this, &ServerConfigDialog::toggleRelativeMouseMoves);
  connect(ui->cbEnableClipboard, &QCheckBox::toggled, this, &ServerConfigDialog::toggleClipboard);
  connect(ui->btnBrowseConfigFile, &QPushButton::clicked, this, &ServerConfigDialog::browseConfigFile);
  connect(ui->groupExternalConfig, &QGroupBox::toggled, this, &ServerConfigDialog::toggleExternalConfig);
  connect(ui->lineConfigFile, &QLineEdit::textChanged, this, &ServerConfigDialog::setServerConfig);

  connect(
      ui->sbClipboardSizeLimit, QOverload<int>::of(&QSpinBox::valueChanged), this,
      &ServerConfigDialog::setClipboardLimit
  );
  connect(
      ui->cbDefaultLockToComputerState, &QCheckBox::toggled, this, &ServerConfigDialog::toggleDefaultLockToComputerState
  );
  connect(ui->cbDisableLockToComputer, &QCheckBox::toggled, this, &ServerConfigDialog::toggleLockToComputer);
  connect(
      &m_computerSetupModel, &ComputerSetupModel::computersChanged, this,
      &ServerConfigDialog::setButtonBoxEnabledButtons
  );
  connect(Settings::instance(), &Settings::settingsWritableChanged, this, &ServerConfigDialog::updateControls);
}

void ServerConfigDialog::updateControls() const
{
  const bool writable = Settings::isWritable();
  ui->cbDefaultLockToComputerState->setEnabled(writable);
  ui->cbDisableLockToComputer->setEnabled(writable);
  ui->cbEnableClipboard->setEnabled(writable);
  ui->sbClipboardSizeLimit->setEnabled(writable);
  ui->rbProtocolBarrier->setEnabled(writable);
  ui->rbProtocolSynergy->setEnabled(writable);
  ui->cbHeartbeat->setEnabled(writable);
  ui->cbRelativeMouseMoves->setEnabled(writable);
  ui->cbSwitchDelay->setEnabled(writable);
  ui->cbWin32KeepForeground->setEnabled(writable);
  ui->cbSwitchDoubleTap->setEnabled(writable);
  ui->sbSwitchDoubleTap->setEnabled(writable && ui->cbSwitchDoubleTap->isChecked());
  ui->sbSwitchDelay->setEnabled(writable && ui->cbSwitchDelay->isChecked());
  ui->groupExternalConfig->setEnabled(writable);
  setButtonBoxEnabledButtons();
}

void ServerConfigDialog::restoreFromDefaults()
{
  m_protocol = networkProtocolFromString(Settings::defaultValue(Settings::Server::Protocol).toString());
  m_enableHeartbeat = Settings::defaultValue(Settings::Server::EnableHeartbeat).toBool();
  m_heartbeatRate = Settings::defaultValue(Settings::Server::Heartbeat).toInt();
  m_relativeMouseMoves = Settings::defaultValue(Settings::Server::RelativeMouseMoves).toBool();
  m_win32keepForeground = Settings::defaultValue(Settings::Server::Win32KeepForeground).toBool();
  m_enableSwitchDelay = Settings::defaultValue(Settings::Server::EnableSwitchDelay).toBool();
  m_switchDelay = Settings::defaultValue(Settings::Server::SwitchDelay).toInt();
  m_enableSwitchDoubleTap = Settings::defaultValue(Settings::Server::EnableSwitchDoubleTap).toBool();
  m_switchDoubleTap = Settings::defaultValue(Settings::Server::SwitchDoubleTap).toInt();
  m_defaultLockToComputerState = Settings::defaultValue(Settings::Server::DefaultLockToComputerState).toBool();
  m_disableLockToComputer = Settings::defaultValue(Settings::Server::DisableLockToComputer).toBool();
  m_enableClipboard = Settings::defaultValue(Settings::Server::EnableClipboard).toBool();
  m_clipboardSize = Settings::defaultValue(Settings::Server::ClipboardSize).toUInt();

  ui->groupExternalConfig->setChecked(Settings::defaultValue(Settings::Server::ExternalConfig).toBool());
  ui->lineConfigFile->setText(Settings::defaultValue(Settings::Server::ExternalConfigFile).toString());

  refreshControls();
  updateControls();
}

void ServerConfigDialog::setServerConfig()
{
  const auto configFile = ui->lineConfigFile->text();
  if (!QFile::exists(configFile)) {
    m_buttonBox->enableSave(false);
    return;
  }
  serverConfig().setConfigFile(configFile);
  setButtonBoxEnabledButtons();
}

bool ServerConfigDialog::addComputer(const QString &clientName, bool doSilent)
{
  bool isAccepted = false;
  Computer newComputer(clientName);

  if (ComputerSettingsDialog dlg(this, &newComputer, &model().m_computers);
      doSilent || dlg.exec() == QDialog::Accepted) {
    model().addComputer(newComputer);
    isAccepted = true;
    if (m_monitorView != nullptr) {
      m_monitorView->setSelectedComputer(newComputer.name());
    }
  }
  return isAccepted;
}

bool ServerConfigDialog::isGeneralConfigModified() const
{
  return m_originalServerConfigIsExternal != Settings::value(Settings::Server::ExternalConfig).toBool() ||
         m_originalServerConfigUsesExternalFile != Settings::value(Settings::Server::ExternalConfigFile).toString() ||
         m_protocol != Settings::networkProtocol() ||
         m_enableClipboard != Settings::value(Settings::Server::EnableClipboard).toBool() ||
         m_clipboardSize != Settings::value(Settings::Server::ClipboardSize).toUInt() ||
         m_enableHeartbeat != Settings::value(Settings::Server::EnableHeartbeat).toBool() ||
         m_heartbeatRate != Settings::value(Settings::Server::Heartbeat).toInt() ||
         m_enableSwitchDelay != Settings::value(Settings::Server::EnableSwitchDelay).toBool() ||
         m_switchDelay != Settings::value(Settings::Server::SwitchDelay).toInt() ||
         m_enableSwitchDoubleTap != Settings::value(Settings::Server::EnableSwitchDoubleTap).toBool() ||
         m_switchDoubleTap != Settings::value(Settings::Server::SwitchDoubleTap).toInt() ||
         m_relativeMouseMoves != Settings::value(Settings::Server::RelativeMouseMoves).toBool() ||
         m_win32keepForeground != Settings::value(Settings::Server::Win32KeepForeground).toBool() ||
         m_disableLockToComputer != Settings::value(Settings::Server::DisableLockToComputer).toBool() ||
         m_defaultLockToComputerState != Settings::value(Settings::Server::DefaultLockToComputerState).toBool();
}

bool ServerConfigDialog::isGeneralConfigDefault() const
{
  return ui->groupExternalConfig->isChecked() == Settings::defaultValue(Settings::Server::ExternalConfig).toBool() &&
         ui->lineConfigFile->text() == Settings::defaultValue(Settings::Server::ExternalConfigFile).toString() &&
         m_protocol == networkProtocolFromString(Settings::defaultValue(Settings::Server::Protocol).toString()) &&
         m_enableClipboard == Settings::defaultValue(Settings::Server::EnableClipboard).toBool() &&
         m_clipboardSize == Settings::defaultValue(Settings::Server::ClipboardSize).toUInt() &&
         m_enableHeartbeat == Settings::defaultValue(Settings::Server::EnableHeartbeat).toBool() &&
         m_heartbeatRate == Settings::defaultValue(Settings::Server::Heartbeat).toInt() &&
         m_enableSwitchDelay == Settings::defaultValue(Settings::Server::EnableSwitchDelay).toBool() &&
         m_switchDelay == Settings::defaultValue(Settings::Server::SwitchDelay).toInt() &&
         m_enableSwitchDoubleTap == Settings::defaultValue(Settings::Server::EnableSwitchDoubleTap).toBool() &&
         m_switchDoubleTap == Settings::defaultValue(Settings::Server::SwitchDoubleTap).toInt() &&
         m_relativeMouseMoves == Settings::defaultValue(Settings::Server::RelativeMouseMoves).toBool() &&
         m_win32keepForeground == Settings::defaultValue(Settings::Server::Win32KeepForeground).toBool() &&
         m_disableLockToComputer == Settings::defaultValue(Settings::Server::DisableLockToComputer).toBool() &&
         m_defaultLockToComputerState == Settings::defaultValue(Settings::Server::DefaultLockToComputerState).toBool();
}

void ServerConfigDialog::setButtonBoxEnabledButtons() const
{
  const bool writable = Settings::isWritable();
  m_buttonBox->enableSave(writable && (isGeneralConfigModified() || !(m_originalServerConfig == m_serverConfig)));
  m_buttonBox->enableReset(writable && (isGeneralConfigModified() || !(m_originalServerConfig == m_serverConfig)));
  m_buttonBox->enableRestoreDefaults(
      writable && !isGeneralConfigDefault() && ui->tabWidget->currentWidget() == ui->tabAdvanced
  );
}
