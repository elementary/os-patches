# The library subdirectories first
subdir('libply')
subdir('libply-splash-core')
subdir('libply-splash-graphics')

# plymouthd
plymouthd_run_dir = plymouth_runtime_dir
plymouthd_spool_dir = '/var/spool/plymouth'
plymouthd_time_dir = plymouth_time_dir

plymouthd_sources = files(
  'main.c',
  'plugins/splash/details/plugin.c',
  'ply-boot-protocol.h',
  'ply-boot-server.c',
  'ply-boot-server.h',
)

plymouthd_deps = [
  libply_dep,
  libply_splash_core_dep,
]

plymouthd_cflags = [
  '-DPLYMOUTH_LOCALE_DIRECTORY="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPLYMOUTH_DRM_ESCROW_DIRECTORY="@0@"'.format(get_option('prefix') / get_option('libexecdir') / 'plymouth'),
  '-DPLYMOUTH_LOG_DIRECTORY="@0@"'.format('/var/log'),
  '-DPLYMOUTH_SPOOL_DIRECTORY="@0@"'.format(plymouthd_spool_dir),
]

plymouthd = executable('plymouthd',
  plymouthd_sources,
  dependencies: plymouthd_deps,
  c_args: plymouthd_cflags,
  export_dynamic: true,
  include_directories: config_h_inc,
  install: true,
  install_dir: get_option('sbindir'),
)

plymouthd_fd_escrow = executable('plymouthd-fd-escrow',
  'plymouthd-fd-escrow.c',
  install: true,
  install_dir: get_option('libexecdir') / 'plymouth',
)

install_data('plymouthd.defaults',
  install_dir: plymouth_policy_dir,
)

install_data('plymouthd.conf',
  install_dir: plymouth_conf_dir,
)

install_emptydir(plymouthd_run_dir)
install_emptydir(plymouthd_spool_dir)
install_emptydir(plymouthd_time_dir)


# These subdirectories last
subdir('plugins')
subdir('client')
if get_option('upstart-monitoring')
  subdir('upstart-bridge')
endif
