libply_splash_core_sources = files(
  'ply-boot-splash.c',
  'ply-device-manager.c',
  'ply-input-device.c',
  'ply-kmsg-reader.c',
  'ply-keyboard.c',
  'ply-pixel-buffer.c',
  'ply-pixel-display.c',
  'ply-renderer.c',
  'ply-rich-text.c',
  'ply-terminal.c',
  'ply-terminal-emulator.c',
  'ply-text-display.c',
  'ply-text-progress-bar.c',
  'ply-text-step-bar.c',
)

libply_splash_core_public_deps = [
  libply_dep,
]

libply_splash_core_private_deps = [
  lm_dep,
  libevdev_dep,
  xkbcommon_dep,
  xkeyboard_config_dep,
]

if libudev_dep.found()
  libply_splash_core_private_deps += libudev_dep
endif

libply_splash_core_cflags = [
  '-DPLYMOUTH_BACKGROUND_COLOR=@0@'.format(get_option('background-color')),
  '-DPLYMOUTH_BACKGROUND_START_COLOR=@0@'.format(get_option('background-start-color-stop')),
  '-DPLYMOUTH_BACKGROUND_END_COLOR=@0@'.format(get_option('background-end-color-stop')),
  '-DPLYMOUTH_PLUGIN_PATH="@0@"'.format(plymouth_plugin_path),
]

libply_splash_core = library('ply-splash-core',
  libply_splash_core_sources,
  dependencies: libply_splash_core_public_deps + libply_splash_core_private_deps,
  c_args: libply_splash_core_cflags,
  include_directories: config_h_inc,
  version: plymouth_soversion,
  install: true,
)

libply_splash_core_dep = declare_dependency(
  link_with: libply_splash_core,
  dependencies: libply_dep,
  include_directories: include_directories('.'),
)

libply_splash_core_headers = files(
  'ply-boot-splash-plugin.h',
  'ply-boot-splash.h',
  'ply-device-manager.h',
  'ply-input-device.h',
  'ply-kmsg-reader.h',
  'ply-keyboard.h',
  'ply-pixel-buffer.h',
  'ply-pixel-display.h',
  'ply-renderer-plugin.h',
  'ply-renderer.h',
  'ply-rich-text.h',
  'ply-terminal.h',
  'ply-terminal-emulator.h',
  'ply-text-display.h',
  'ply-text-progress-bar.h',
  'ply-text-step-bar.h',
)

install_headers(libply_splash_core_headers,
  install_dir: get_option('includedir') / 'plymouth-1' / 'ply-splash-core',
)

pkgconfig.generate(libply_splash_core,
  filebase: 'ply-splash-core',
  name: 'Plymouth',
  description: 'Utility Library for Boot Splash Plugins',
  libraries: [
    libply,
    ldl_dep,
  ],
  subdirs: [
    'plymouth-1/ply',
    'plymouth-1/ply-splash-core',
  ],
  variables: {
    'pluginsdir': plymouth_plugin_path,
    'themesdir': plymouth_theme_path,
    'confdir': plymouth_conf_dir,
    'policydir': plymouth_policy_dir,
  },
)
