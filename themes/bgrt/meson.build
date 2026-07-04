bgrt_plymouth = i18n.merge_file(
  input: configure_file(
    input: 'bgrt.plymouth.desktop',
    output: 'bgrt.plymouth.desktop',
    configuration: {
      'PLYMOUTH_THEME_PATH': plymouth_theme_path,
    },
  ),
  output: 'bgrt.plymouth',
  type: 'desktop',
  po_dir: po_dir,
  install: true,
  install_dir: plymouth_theme_path / 'bgrt',
)
