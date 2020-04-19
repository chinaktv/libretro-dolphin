// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/NetworkWidget.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "Core/ConfigManager.h"
#include "Core/IOS/Network/SSL.h"
#include "Core/IOS/Network/Socket.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

namespace
{
QTableWidgetItem* GetSocketDomain(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr sa;
  socklen_t sa_len = sizeof(sa);
  const int ret = getsockname(host_fd, &sa, &sa_len);
  if (ret != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  switch (sa.sa_family)
  {
  case 2:
    return new QTableWidgetItem(QLatin1Literal("AF_INET"));
  case 23:
    return new QTableWidgetItem(QLatin1Literal("AF_INET6"));
  default:
    return new QTableWidgetItem(QString::number(sa.sa_family));
  }
}

QTableWidgetItem* GetSocketType(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  int so_type;
  socklen_t opt_len = sizeof(so_type);
  const int ret =
      getsockopt(host_fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&so_type), &opt_len);
  if (ret != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  switch (so_type)
  {
  case 1:
    return new QTableWidgetItem(QLatin1Literal("SOCK_STREAM"));
  case 2:
    return new QTableWidgetItem(QLatin1Literal("SOCK_DGRAM"));
  default:
    return new QTableWidgetItem(QString::number(so_type));
  }
}

QTableWidgetItem* GetSocketState(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(sockaddr_in);
  if (getpeername(host_fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_addr_len) == 0)
    return new QTableWidgetItem(QTableWidget::tr("Connected"));

  int so_accept = 0;
  socklen_t opt_len = sizeof(so_accept);
  const int ret =
      getsockopt(host_fd, SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<char*>(&so_accept), &opt_len);
  if (ret == 0 && so_accept > 0)
    return new QTableWidgetItem(QTableWidget::tr("Listening"));
  return new QTableWidgetItem(QTableWidget::tr("Unbound"));
}

QTableWidgetItem* GetSocketName(s32 host_fd)
{
  if (host_fd < 0)
    return new QTableWidgetItem();

  sockaddr_in sock_addr;
  socklen_t sock_addr_len = sizeof(sockaddr_in);
  if (getsockname(host_fd, reinterpret_cast<sockaddr*>(&sock_addr), &sock_addr_len) != 0)
    return new QTableWidgetItem(QTableWidget::tr("Unknown"));

  const QString sock_name = QStringLiteral("%1:%2")
                                .arg(QString::fromLatin1(inet_ntoa(sock_addr.sin_addr)))
                                .arg(ntohs(sock_addr.sin_port));

  sockaddr_in peer_addr;
  socklen_t peer_addr_len = sizeof(sockaddr_in);
  if (getpeername(host_fd, reinterpret_cast<sockaddr*>(&peer_addr), &peer_addr_len) != 0)
    return new QTableWidgetItem(sock_name);

  const QString peer_name = QStringLiteral("%1:%2")
                                .arg(QString::fromLatin1(inet_ntoa(peer_addr.sin_addr)))
                                .arg(ntohs(peer_addr.sin_port));
  return new QTableWidgetItem(QStringLiteral("%1->%2").arg(sock_name).arg(peer_name));
}
}  // namespace

NetworkWidget::NetworkWidget(QWidget* parent) : QDockWidget(parent)
{
  setWindowTitle(tr("Network"));
  setObjectName(QStringLiteral("network"));

  setHidden(!Settings::Instance().IsNetworkVisible() || !Settings::Instance().IsDebugModeEnabled());

  setAllowedAreas(Qt::AllDockWidgetAreas);

  CreateWidgets();

  auto& settings = Settings::GetQSettings();

  restoreGeometry(settings.value(QStringLiteral("networkwidget/geometry")).toByteArray());
  // macOS: setHidden() needs to be evaluated before setFloating() for proper window presentation
  // according to Settings
  setFloating(settings.value(QStringLiteral("networkwidget/floating")).toBool());

  ConnectWidgets();

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this, &NetworkWidget::Update);

  connect(&Settings::Instance(), &Settings::NetworkVisibilityChanged,
          [this](bool visible) { setHidden(!visible); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, [this](bool enabled) {
    setHidden(!enabled || !Settings::Instance().IsNetworkVisible());
  });
}

NetworkWidget::~NetworkWidget()
{
  auto& settings = Settings::GetQSettings();

  settings.setValue(QStringLiteral("networkwidget/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("networkwidget/floating"), isFloating());
}

void NetworkWidget::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetNetworkVisible(false);
}

void NetworkWidget::showEvent(QShowEvent* event)
{
  Update();
}

void NetworkWidget::CreateWidgets()
{
  auto* widget = new QWidget;
  auto* layout = new QVBoxLayout;
  widget->setLayout(layout);
  layout->addWidget(CreateSocketTableGroup());
  layout->addWidget(CreateSSLContextGroup());
  layout->addWidget(CreateSSLOptionsGroup());
  layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
  setWidget(widget);

  Update();
}

void NetworkWidget::ConnectWidgets()
{
  connect(m_dump_ssl_read_checkbox, &QCheckBox::stateChanged,
          [](int state) { SConfig::GetInstance().m_SSLDumpRead = state == Qt::Checked; });
  connect(m_dump_ssl_write_checkbox, &QCheckBox::stateChanged,
          [](int state) { SConfig::GetInstance().m_SSLDumpWrite = state == Qt::Checked; });
  connect(m_dump_root_ca_checkbox, &QCheckBox::stateChanged,
          [](int state) { SConfig::GetInstance().m_SSLDumpRootCA = state == Qt::Checked; });
  connect(m_dump_peer_cert_checkbox, &QCheckBox::stateChanged,
          [](int state) { SConfig::GetInstance().m_SSLDumpPeerCert = state == Qt::Checked; });
  connect(m_verify_certificates_checkbox, &QCheckBox::stateChanged,
          [](int state) { SConfig::GetInstance().m_SSLVerifyCert = state == Qt::Checked; });
}

void NetworkWidget::Update()
{
  m_socket_table->setRowCount(0);
  for (u32 wii_fd = 0; wii_fd < IOS::HLE::WII_SOCKET_FD_MAX; wii_fd++)
  {
    m_socket_table->insertRow(wii_fd);
    const s32 host_fd = IOS::HLE::WiiSockMan::GetInstance().GetHostSocket(wii_fd);
    m_socket_table->setItem(wii_fd, 0, new QTableWidgetItem(QString::number(wii_fd)));
    m_socket_table->setItem(wii_fd, 1, GetSocketDomain(host_fd));
    m_socket_table->setItem(wii_fd, 2, GetSocketType(host_fd));
    m_socket_table->setItem(wii_fd, 3, GetSocketState(host_fd));
    m_socket_table->setItem(wii_fd, 4, GetSocketName(host_fd));
  }
  m_socket_table->resizeColumnsToContents();

  m_ssl_table->setRowCount(0);
  for (u32 ssl_id = 0; ssl_id < IOS::HLE::NET_SSL_MAXINSTANCES; ssl_id++)
  {
    m_ssl_table->insertRow(ssl_id);
    s32 host_fd = -1;
    if (IOS::HLE::Device::IsSSLIDValid(ssl_id) &&
        IOS::HLE::Device::NetSSL::_SSL[ssl_id].ctx.p_bio != nullptr)
    {
      host_fd =
          static_cast<mbedtls_net_context*>(IOS::HLE::Device::NetSSL::_SSL[ssl_id].ctx.p_bio)->fd;
    }
    m_ssl_table->setItem(ssl_id, 0, new QTableWidgetItem(QString::number(ssl_id)));
    m_ssl_table->setItem(ssl_id, 1, GetSocketDomain(host_fd));
    m_ssl_table->setItem(ssl_id, 2, GetSocketType(host_fd));
    m_ssl_table->setItem(ssl_id, 3, GetSocketState(host_fd));
    m_ssl_table->setItem(ssl_id, 4, GetSocketName(host_fd));
  }
  m_ssl_table->resizeColumnsToContents();

  const auto& config = SConfig::GetInstance();
  m_dump_ssl_read_checkbox->setChecked(config.m_SSLDumpRead);
  m_dump_ssl_write_checkbox->setChecked(config.m_SSLDumpWrite);
  m_dump_root_ca_checkbox->setChecked(config.m_SSLDumpRootCA);
  m_dump_peer_cert_checkbox->setChecked(config.m_SSLDumpPeerCert);
  m_verify_certificates_checkbox->setChecked(config.m_SSLVerifyCert);
}

QGroupBox* NetworkWidget::CreateSocketTableGroup()
{
  QGroupBox* socket_table_group = new QGroupBox(tr("Socket table"));
  QGridLayout* socket_table_layout = new QGridLayout;
  socket_table_group->setLayout(socket_table_layout);

  m_socket_table = new QTableWidget();
  QStringList header{tr("FD"), tr("Domain"), tr("Type"), tr("State"), tr("Name")};
  m_socket_table->setColumnCount(header.size());

  m_socket_table->setHorizontalHeaderLabels(header);
  m_socket_table->setTabKeyNavigation(false);
  m_socket_table->verticalHeader()->setVisible(false);
  m_socket_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_socket_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_socket_table->setWordWrap(false);

  socket_table_layout->addWidget(m_socket_table, 0, 0);
  socket_table_layout->setSpacing(1);
  return socket_table_group;
}

QGroupBox* NetworkWidget::CreateSSLContextGroup()
{
  QGroupBox* ssl_context_group = new QGroupBox(tr("SSL context"));
  QGridLayout* ssl_context_layout = new QGridLayout;
  ssl_context_group->setLayout(ssl_context_layout);

  m_ssl_table = new QTableWidget();
  QStringList header{tr("ID"), tr("Domain"), tr("Type"), tr("State"), tr("Name")};
  m_ssl_table->setColumnCount(header.size());

  m_ssl_table->setHorizontalHeaderLabels(header);
  m_ssl_table->setTabKeyNavigation(false);
  m_ssl_table->verticalHeader()->setVisible(false);
  m_ssl_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_ssl_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_ssl_table->setWordWrap(false);

  ssl_context_layout->addWidget(m_ssl_table, 0, 0);
  ssl_context_layout->setSpacing(1);
  return ssl_context_group;
}

QGroupBox* NetworkWidget::CreateSSLOptionsGroup()
{
  QGroupBox* ssl_options_group = new QGroupBox(tr("SSL options"));
  QGridLayout* ssl_options_layout = new QGridLayout;
  ssl_options_group->setLayout(ssl_options_layout);

  m_dump_ssl_read_checkbox = new QCheckBox(tr("Dump SSL read"));
  m_dump_ssl_write_checkbox = new QCheckBox(tr("Dump SSL write"));
  m_dump_root_ca_checkbox = new QCheckBox(tr("Dump root CA"));
  m_dump_peer_cert_checkbox = new QCheckBox(tr("Dump peer certificates"));
  m_verify_certificates_checkbox = new QCheckBox(tr("Verify certificates"));

  ssl_options_layout->addWidget(m_dump_ssl_read_checkbox, 0, 0);
  ssl_options_layout->addWidget(m_dump_ssl_write_checkbox, 1, 0);
  ssl_options_layout->addWidget(m_verify_certificates_checkbox, 2, 0);
  ssl_options_layout->addWidget(m_dump_root_ca_checkbox, 0, 1);
  ssl_options_layout->addWidget(m_dump_peer_cert_checkbox, 1, 1);

  ssl_options_layout->setSpacing(1);
  return ssl_options_group;
}
