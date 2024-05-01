space_flares_plugin = shared_module('space-flares',
  'plugin.c',
  dependencies: [
    libply_splash_core_dep,
    libply_splash_graphics_dep,
  ],
  c_args: [
    '-DPLYMOUTH_LOGO_FILE="@0@"'.format(plymouth_logo_file),
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: true,
  install_dir: plymouth_plugin_path,
)
