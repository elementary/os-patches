sources = files(
  'ce-page.c',
  'ce-polkit-button.c',
  'ce-polkit.c',
  'connection-helpers.c',
  'ip4-routes-dialog.c',
  'ip6-routes-dialog.c',
  'nm-connection-editor.c',
  'nm-connection-list.c',
  'main.c',
  'page-8021x-security.c',
  'page-bridge.c',
  'page-bridge-port.c',
  'page-bluetooth.c',
  'page-bond.c',
  'page-dcb.c',
  'page-dsl.c',
  'page-ethernet.c',
  'page-general.c',
  'page-infiniband.c',
  'page-ip-tunnel.c',
  'page-ip4.c',
  'page-ip6.c',
  'page-macsec.c',
  'page-master.c',
  'page-mobile.c',
  'page-ppp.c',
  'page-proxy.c',
  'page-team.c',
  'page-team-port.c',
  'page-vlan.c',
  'page-vpn.c',
  'page-wifi.c',
  'page-wifi-security.c',
  'ppp-auth-methods-dialog.c',
  'vpn-helpers.c'
)

resource_data = files(
  'ce-ip4-routes.ui',
  'ce-ip6-routes.ui',
  'ce-new-connection.ui',
  'ce-page-bluetooth.ui',
  'ce-page-bond.ui',
  'ce-page-bridge-port.ui',
  'ce-page-bridge.ui',
  'ce-page-dcb.ui',
  'ce-page-dsl.ui',
  'ce-page-ethernet.ui',
  'ce-page-general.ui',
  'ce-page-infiniband.ui',
  'ce-page-ip4.ui',
  'ce-page-ip6.ui',
  'ce-page-ip-tunnel.ui',
  'ce-page-macsec.ui',
  'ce-page-mobile.ui',
  'ce-page-ppp.ui',
  'ce-page-proxy.ui',
  'ce-page-team-port.ui',
  'ce-page-team.ui',
  'ce-page-vlan.ui',
  'ce-page-wifi-security.ui',
  'ce-page-wifi.ui',
  'ce-ppp-auth-methods.ui',
  'nm-connection-editor.ui',
  'nm-connection-list.ui'
)

sources += gnome.compile_resources(
  'ce-resources',
  'ce.gresource.xml',
  dependencies: resource_data
)

incs = [
  top_inc,
  utils_inc,
  src_inc,
  shared_inc,
  wireless_security_inc
]

deps = [
  gtk_dep,
  libnm_dep,
  libnma_dep,
  m_dep
]

cflags = [
  '-DBINDIR="@0@"'.format(nma_bindir),
  '-DDATADIR="@0@"'.format(nma_datadir),
  '-DLIBDIR="@0@"'.format(nma_libdir),
  '-DNMALOCALEDIR="@0@"'.format(nma_localedir),
  '-DSYSCONFDIR="@0@"'.format(nma_sysconfdir),
  '-DICONDIR="@0@"'.format(nma_icondir)
]

ldflags = []
if have_version_script
  ldflags += '-Wl,--version-script,@0@'.format(linker_script_ver)
endif

if enable_selinux
  deps += libselinux_dep
endif

if enable_team
  deps += jansson_dep
endif

executable(
  'nm-connection-editor',
  sources,
  include_directories: incs,
  dependencies: deps,
  c_args: cflags,
  link_args: ldflags,
  link_depends: linker_script_ver,
  link_whole: libwireless_security_libnm,
  install: true,
  install_dir: nma_bindir
)
