project('iio-sensor-proxy', [ 'c' ],
        version: '3.3',
        license: 'GPLv3+',
        default_options: [
          'buildtype=debugoptimized',
          'warning_level=1',
          'c_std=c99',
        ],
        meson_version: '>= 0.54.0')

cc = meson.get_compiler('c')

common_cflags = cc.get_supported_arguments([
    '-fgnu89-inline',
    '-fvisibility=hidden',
    '-std=gnu99',
    '-Wall',
    '-Wundef',
    '-Wunused',
    '-Wstrict-prototypes',
    '-Werror-implicit-function-declaration',
    '-Wno-pointer-sign',
    '-Wshadow'
])

prefix = get_option('prefix')
libexecdir = prefix / get_option('libexecdir')
bindir = get_option('bindir')
dbusconfdir = get_option('sysconfdir') / 'dbus-1' / 'system.d'

mathlib_dep = cc.find_library('m', required: false)
udev_rules_dir = get_option('udevrulesdir')
if udev_rules_dir == 'auto'
    udev_dep = dependency('udev')
    udev_rules_dir = udev_dep.get_pkgconfig_variable('udevdir') + '/rules.d'
endif
systemd_system_unit_dir = get_option('systemdsystemunitdir')
if systemd_system_unit_dir == 'auto'
    systemd_dep = dependency('systemd')
    systemd_system_unit_dir = systemd_dep.get_pkgconfig_variable('systemdsystemunitdir')
endif
if get_option('gtk-tests')
    gtk_dep = dependency('gtk+-3.0', required: false)
endif
gio_dep = dependency('gio-2.0')
gudev_dep = dependency('gudev-1.0', version: '>= 237')

gnome = import('gnome')

add_global_arguments('-D_GNU_SOURCE=1', language: 'c')

subdir('src')
subdir('data')

if get_option('gtk_doc')
  # Make like license available in the build root for docs
  configure_file(
    input: 'COPYING',
    output: 'COPYING',
    copy: true,
  )
  subdir('docs')
endif

subdir('tests')
