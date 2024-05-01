systemd_unit_templates = [
  'plymouth-switch-root.service.in',
  'plymouth-switch-root-initramfs.service.in',
  'plymouth-start.service.in',
  'plymouth-read-write.service.in',
  'plymouth-quit.service.in',
  'plymouth-quit-wait.service.in',
  'plymouth-reboot.service.in',
  'plymouth-kexec.service.in',
  'plymouth-poweroff.service.in',
  'plymouth-halt.service.in',
  'systemd-ask-password-plymouth.path.in',
  'systemd-ask-password-plymouth.service.in',
]

foreach unit_templ : systemd_unit_templates
  configure_file(
    input: unit_templ,
    output: '@BASENAME@',
    configuration: {
      'PLYMOUTH_CLIENT_DIR': get_option('prefix') / get_option('bindir'),
      'PLYMOUTH_DAEMON_DIR': get_option('prefix') / get_option('sbindir'),
      'plymouthruntimedir': plymouth_runtime_dir,
      'SYSTEMD_ASK_PASSWORD_AGENT': systemd_ask_password_agent.full_path(),
    },
    install: true,
    install_dir: systemd_unit_dir,
  )
endforeach

systemd_target_links = [
  {
    'target': 'initrd-switch-root.target.wants',
    'services': [ 'plymouth-start', 'plymouth-switch-root' ],
  },
  {
    'target': 'sysinit.target.wants',
    'services': [ 'plymouth-start', 'plymouth-read-write' ],
  },
  {
    'target': 'multi-user.target.wants',
    'services': [ 'plymouth-quit', 'plymouth-quit-wait' ],
  },
  {
    'target': 'reboot.target.wants',
    'services': [ 'plymouth-reboot', 'plymouth-switch-root-initramfs' ],
  },
  {
    'target': 'kexec.target.wants',
    'services': [ 'plymouth-kexec', 'plymouth-switch-root-initramfs' ],
  },
  {
    'target': 'poweroff.target.wants',
    'services': [ 'plymouth-poweroff', 'plymouth-switch-root-initramfs' ],
  },
  {
    'target': 'halt.target.wants',
    'services': [ 'plymouth-halt', 'plymouth-switch-root-initramfs' ],
  },
]

foreach entry : systemd_target_links
  foreach service : entry['services']
    install_symlink(
      service + '.service',
      install_dir: systemd_unit_dir / entry['target'],
      pointing_to: '..' / service + '.service',
    )
  endforeach
endforeach
