libply_splash_graphics_sources = files(
  'ply-animation.c',
  'ply-capslock-icon.c',
  'ply-console-viewer.c',
  'ply-entry.c',
  'ply-image.c',
  'ply-keymap-icon.c',
  'ply-label.c',
  'ply-progress-animation.c',
  'ply-progress-bar.c',
  'ply-throbber.c',
)

libply_splash_graphics_deps = [
  libply_splash_core_dep,
  lm_dep,
  libpng_dep,
]

libply_splash_graphics_cflags = [
  '-DPLYMOUTH_BACKGROUND_COLOR=@0@'.format(get_option('background-color')),
  '-DPLYMOUTH_BACKGROUND_START_COLOR=@0@'.format(get_option('background-start-color-stop')),
  '-DPLYMOUTH_BACKGROUND_END_COLOR=@0@'.format(get_option('background-end-color-stop')),
  '-DPLYMOUTH_PLUGIN_PATH="@0@"'.format(plymouth_plugin_path),
]

libply_splash_graphics = library('ply-splash-graphics',
  libply_splash_graphics_sources,
  dependencies: libply_splash_graphics_deps,
  c_args: libply_splash_graphics_cflags,
  include_directories: config_h_inc,
  version: plymouth_soversion,
  install: true,
)

libply_splash_graphics_dep = declare_dependency(
  link_with: libply_splash_graphics,
  include_directories: include_directories('.'),
)

libply_splash_graphics_headers = files(
  'ply-animation.h',
  'ply-capslock-icon.h',
  'ply-console-viewer.h',
  'ply-entry.h',
  'ply-image.h',
  'ply-keymap-icon.h',
  'ply-keymap-metadata.h',
  'ply-label-plugin.h',
  'ply-label.h',
  'ply-progress-animation.h',
  'ply-progress-bar.h',
  'ply-throbber.h',
)

install_headers(libply_splash_graphics_headers,
  install_dir: get_option('includedir') / 'plymouth-1' / 'ply-splash-graphics',
)

pkgconfig.generate(libply_splash_graphics,
  filebase: 'ply-splash-graphics',
  name: 'Plymouth',
  description: 'Graphics Utility Library for Boot Splash Plugins',
  requires: [
    'ply-splash-core',
  ],
  subdirs: [
    'plymouth-1/ply-splash-graphics',
  ],
  variables: {
    'pluginsdir': plymouth_plugin_path,
    'themesdir': plymouth_theme_path,
  },
)
