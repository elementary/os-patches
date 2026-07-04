fade_in_plymouth = configure_file(
  input: 'fade-in.plymouth.in',
  output: '@BASENAME@',
  configuration: {
    'PLYMOUTH_THEME_PATH': plymouth_theme_path,
  },
  install: true,
  install_dir: plymouth_theme_path / 'fade-in',
)

install_data(
  'star.png',
  'bullet.png',
  'entry.png',
  'lock.png',
  install_dir: plymouth_theme_path / 'fade-in',
)
