x11_plugin = shared_module('x11',
  'plugin.c',
  dependencies: [
    libply_dep,
    libply_splash_core_dep,
    gtk3_dep,
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: true,
  install_dir: plymouth_plugin_path / 'renderers',
)
