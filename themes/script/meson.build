script_plymouth = configure_file(
  input: 'script.plymouth.in',
  output: '@BASENAME@',
  configuration: {
    'PLYMOUTH_THEME_PATH': plymouth_theme_path,
  },
  install: true,
  install_dir: plymouth_theme_path / 'script',
)

install_data(
  'script.script',
  'box.png',
  'bullet.png',
  'entry.png',
  'lock.png',
  'progress_box.png',
  'progress_bar.png',
  install_dir: plymouth_theme_path / 'script',
)
