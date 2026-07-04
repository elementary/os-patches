plymouth_upstart_bridge_src = files(
  'ply-upstart-monitor.c',
  'plymouth-upstart-bridge.c',
)

plymouth_upstart_bridge_deps = [
  libply_dep,
  libply_boot_client_dep,
  dbus_dep,
  curses_dep,
]

plymouth_upstart_bridge = executable('plymouth-upstart-bridge',
  plymouth_upstart_bridge_src,
  dependencies: plymouth_upstart_bridge_deps,
  include_directories: config_h_inc,
  install: true,
)
