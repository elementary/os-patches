solar_plymouth = configure_file(
  input: 'solar.plymouth.in',
  output: '@BASENAME@',
  configuration: {
    'PLYMOUTH_THEME_PATH': plymouth_theme_path,
  },
  install: true,
  install_dir: plymouth_theme_path / 'solar',
)

install_data(
  'box.png',
  'bullet.png',
  'entry.png',
  'lock.png',
  'progress_bar.png',
  'star.png',
  install_dir: plymouth_theme_path / 'solar',
)
