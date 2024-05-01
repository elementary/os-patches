frame_buffer_plugin = shared_module('frame-buffer',
  'plugin.c',
  dependencies: [
    libply_dep,
    libply_splash_core_dep,
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: true,
  install_dir: plymouth_plugin_path / 'renderers',
)
