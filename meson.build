project('plymouth', 'c',
  meson_version: '>= 0.61',
  version: run_command(['scripts/generate-version.sh'], check: true).stdout().strip(),
)

# Modules
i18n = import('i18n')
pkgconfig = import('pkgconfig')

# General variables
plymouth_soversion = '5.0.0'

plymouth_theme_path = get_option('prefix') / get_option('datadir') / 'plymouth' / 'themes/'
plymouth_plugin_path = get_option('prefix') / get_option('libdir') / 'plymouth/'
plymouth_policy_dir = get_option('prefix') / get_option('datadir') / 'plymouth/'
plymouth_conf_dir = get_option('prefix') / get_option('sysconfdir') / 'plymouth/'
plymouth_time_dir = get_option('prefix') / get_option('localstatedir') / 'lib' / 'plymouth'

plymouth_runtime_dir = get_option('runstatedir') / 'plymouth'
plymouth_runtime_theme_path = plymouth_runtime_dir / 'themes/'

# Dependencies
cc = meson.get_compiler('c')
lm_dep = cc.find_library('m')
lrt_dep = cc.find_library('rt')

ldl_dep = dependency('dl')

libpng_dep = dependency('libpng', version: '>= 1.2.16')

libudev_dep = dependency('libudev', required: get_option('udev'))
libpango_dep = dependency('pango', required: get_option('pango'))
libcairo_dep = dependency('cairo', required: get_option('pango'))
libpangocairo_dep = dependency('pangocairo', required: get_option('pango'))
libfreetype_dep = dependency('freetype2', required: get_option('freetype'))
gtk3_dep = dependency('gtk+-3.0', version: '>= 3.14.0', required: get_option('gtk'))
libdrm_dep = dependency('libdrm', required: get_option('drm'))
libevdev_dep = dependency('libevdev')
xkbcommon_dep = dependency('xkbcommon')
xkeyboard_config_dep = dependency('xkeyboard-config')

if get_option('systemd-integration')
  systemd_dep = dependency('systemd')
  systemd_unit_dir = systemd_dep.get_variable('systemdsystemunitdir',
    pkgconfig_define: [ 'rootprefix', get_option('prefix') ],
  )
  systemd_ask_password_agent = find_program('systemd-tty-ask-password-agent')
endif

if get_option('upstart-monitoring')
  dbus_dep = dependency('dbus-1')
  curses_dep = dependency('curses')
endif

# Logo
plymouth_logo_file = get_option('logo')
use_fallback_logo = plymouth_logo_file == ''
if use_fallback_logo
  plymouth_logo_file = get_option('prefix') / get_option('datadir') / 'plymouth' / 'bizcom.png'
endif

# Global C flags
add_project_arguments([
    '-D_GNU_SOURCE',
    '-include', 'config.h',
  ],
  language: 'c'
)

# config.h
conf = configuration_data()
conf.set_quoted('BOOT_TTY', get_option('boot-tty'))
conf.set_quoted('SHUTDOWN_TTY', get_option('shutdown-tty'))
conf.set_quoted('RELEASE_FILE', get_option('release-file'))
conf.set('HAVE_UDEV', libudev_dep.found())
conf.set('PLY_ENABLE_SYSTEMD_INTEGRATION', get_option('systemd-integration'))
conf.set('PLY_ENABLE_TRACING', get_option('tracing'))
conf.set_quoted('PLYMOUTH_RUNTIME_DIR', plymouth_runtime_dir)
conf.set_quoted('PLYMOUTH_THEME_PATH', plymouth_theme_path)
conf.set_quoted('PLYMOUTH_RUNTIME_THEME_PATH', plymouth_runtime_theme_path)
conf.set_quoted('PLYMOUTH_PLUGIN_PATH', plymouth_plugin_path)
conf.set_quoted('PLYMOUTH_POLICY_DIR', plymouth_policy_dir)
conf.set_quoted('PLYMOUTH_CONF_DIR', plymouth_conf_dir)
conf.set_quoted('PLYMOUTH_TIME_DIRECTORY', plymouth_time_dir)
conf.set('HAVE_NCURSESW_TERM_H', get_option('upstart-monitoring')? cc.has_header('ncursesw/term.h') : false)
conf.set('HAVE_NCURSES_TERM_H', get_option('upstart-monitoring')? cc.has_header('ncurses/term.h') : false)
config_file = configure_file(
  output: 'config.h',
  configuration: conf,
)
config_h_inc = include_directories('.')

# Subdirectories
subdir('images')
subdir('po')
subdir('src')
subdir('themes')
subdir('scripts')
if get_option('systemd-integration')
  subdir('systemd-units')
endif
if get_option('docs')
  subdir('docs')
endif
