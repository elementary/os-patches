configure_file(
  input: 'plymouth-generate-initrd.in',
  output: '@BASENAME@',
  configuration: {
    'PLYMOUTH_LIBEXECDIR': get_option('prefix') / get_option('libexecdir'),
    'PLYMOUTH_DATADIR': get_option('prefix') / get_option('datadir'),
  },
  install_dir: get_option('libexecdir') / 'plymouth',
  install_mode: 'rwxr-xr-x',
)

configure_file(
  input: 'plymouth-populate-initrd.in',
  output: '@BASENAME@',
  configuration: {
    'PLYMOUTH_LIBEXECDIR': get_option('prefix') / get_option('libexecdir'),
    'PLYMOUTH_DATADIR': get_option('prefix') / get_option('datadir'),
    'PLYMOUTH_LOGO_FILE': plymouth_logo_file,
    'PLYMOUTH_CONF_DIR': plymouth_conf_dir,
    'PLYMOUTH_POLICY_DIR': plymouth_policy_dir,
    'PLYMOUTH_CLIENT_DIR': get_option('prefix') / get_option('bindir'),
    'PLYMOUTH_DAEMON_DIR': get_option('prefix') / get_option('sbindir'),
    'SYSTEMD_UNIT_DIR': get_option('systemd-integration')? systemd_unit_dir : '',
    'RELEASE_FILE': get_option('release-file'),
  },
  install_dir: get_option('libexecdir') / 'plymouth',
  install_mode: 'rwxr-xr-x',
)

install_data(
  'plymouth-update-initrd',
  install_dir: get_option('libexecdir') / 'plymouth',
  install_mode: 'rwxr-xr-x',
)

configure_file(
  input: 'plymouth-set-default-theme.in',
  output: '@BASENAME@',
  configuration: {
    'PLYMOUTH_LIBDIR': get_option('prefix') / get_option('libdir'),
    'PLYMOUTH_LIBEXECDIR': get_option('prefix') / get_option('libexecdir'),
    'PLYMOUTH_DATADIR': get_option('prefix') / get_option('datadir'),
    'PLYMOUTH_CONF_DIR': plymouth_conf_dir,
    'PLYMOUTH_POLICY_DIR': plymouth_policy_dir,
  },
  install_dir: get_option('sbindir'),
  install_mode: 'rwxr-xr-x',
)

install_data(
  'bootlog',
  install_dir: get_option('sysconfdir') / 'logrotate.d',
)
