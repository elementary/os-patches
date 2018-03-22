project ('libcloudproviders', 'c',
  version: '0.2.5',
  meson_version: '>=0.40.0',
  license: 'LGPL3+',
  default_options: [
    'c_std=gnu99',
    'warning_level=2',
  ]
)

cc = meson.get_compiler('c')

test_cflags = [
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers',
  '-Wstrict-prototypes',
  '-Wmissing-prototypes',
  '-Werror=implicit-function-declaration',
  '-Werror=pointer-arith',
  '-Werror=init-self',
  ['-Werror=format-security','-Werror=format=1'],
  '-Werror=date-time',
]
global_cflags = []
foreach cflag : test_cflags
  if cc.has_multi_arguments(cflag)
    global_cflags += cflag
  endif
endforeach
add_project_arguments(global_cflags, language: 'c')

glib_ver = '>= 2.51.2'
glib = dependency ('glib-2.0', version: glib_ver)
gio = dependency ('gio-2.0', version: glib_ver)
gio_unix = dependency ('gio-unix-2.0', version: glib_ver)

add_project_arguments(
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_52',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_52',
  language: 'c'
)

prefix = get_option ('prefix')
libdir = get_option ('libdir')
libexecdir = get_option ('libexecdir')
bindir = get_option ('bindir')
datadir = get_option ('datadir')
servicedir = join_paths (datadir, 'dbus-1', 'services')
incdir = join_paths (prefix, get_option('includedir'), 'cloudproviders')

gnome = import('gnome')

subdir ('src')
subdir ('test')
if get_option ('enable-gtk-doc')
    subdir ('docs/reference')
endif
