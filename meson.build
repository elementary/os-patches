project(
  'gnome-bluetooth', 'c',
  version: '3.26.1',
  license: 'GPL2+',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=1'
  ],
  meson_version: '>= 0.41.0'
)

gnomebt_version = meson.project_version()
version_array = gnomebt_version.split('.')
gnomebt_major_version = version_array[0].to_int()
gnomebt_minor_version = version_array[1].to_int()
gnomebt_micro_version = version_array[2].to_int()

gnomebt_api_version = '1.0'
gnomebt_api_name = '@0@-@1@'.format(meson.project_name(), gnomebt_api_version)

gnomebt_gir_ns = 'GnomeBluetooth'
gnomebt_gir_version = '1.0'

gnomebt_gettext_package = meson.project_name() + '2'

gnomebt_prefix = get_option('prefix')
gnomebt_bindir = join_paths(gnomebt_prefix, get_option('bindir'))
gnomebt_datadir = join_paths(gnomebt_prefix, get_option('datadir'))
gnomebt_includedir = join_paths(gnomebt_prefix, get_option('includedir'))
gnomebt_libdir = join_paths(gnomebt_prefix, get_option('libdir'))
gnomebt_libexecdir = join_paths(gnomebt_prefix, get_option('libexecdir'))
gnomebt_localedir = join_paths(gnomebt_prefix, get_option('localedir'))
gnomebt_mandir = join_paths(gnomebt_prefix, get_option('mandir'))

gnomebt_pkgdatadir = join_paths(gnomebt_datadir, meson.project_name())
gnomebt_pkglibdir = join_paths(gnomebt_libdir, meson.project_name())

gnomebt_icondir = join_paths(gnomebt_datadir, 'icons')

gnomebt_buildtype = get_option('buildtype')
gnomebt_debug = gnomebt_buildtype.contains('debug')

# options
enable_gtk_doc = get_option('enable-gtk-doc')
enable_gir = get_option('enable-introspection')

# Before making a release, the GNOMEBT_LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
current = 13
revision = 1
age = 0

libversion = '@0@.@1@.@2@'.format(current, age, revision)

cc = meson.get_compiler('c')

config_h = configuration_data()

# defines
set_defines = [
  # package
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_BUGREPORT', 'http://bugzilla.gnome.org/enter_bug.cgi?product=' + meson.project_name()],
  ['PACKAGE_NAME', meson.project_name()],
  ['PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), gnomebt_version)],
  ['PACKAGE_TARNAME', meson.project_name()],
  ['PACKAGE_URL', ''],
  ['PACKAGE_VERSION', gnomebt_version],
  ['VERSION', gnomebt_version],
  # i18n
  ['GETTEXT_PACKAGE', gnomebt_gettext_package],
  ['LOCALEDIR', gnomebt_localedir]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# debug
config_h.set('NDEBUG', not gnomebt_debug)
config_h.set('GNOME_ENABLE_DEBUG', gnomebt_debug)

# headers
check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_UNISTD_H', 'unistd.h']
]

foreach header: check_headers
  config_h.set(header[0], cc.has_header(header[1]))
endforeach

# compiler flags
common_flags = [
  '-DHAVE_CONFIG_H',
  '-DBONOBO_DISABLE_DEPRECATED',
  '-DBONOBO_DISABLE_SINGLE_INCLUDES',
  '-DBONOBO_UI_DISABLE_DEPRECATED',
  '-DBONOBO_UI_DISABLE_SINGLE_INCLUDES',
  '-DGCONF_DISABLE_DEPRECATED',
  '-DGCONF_DISABLE_SINGLE_INCLUDES',
  '-DGNOME_DISABLE_DEPRECATED',
  '-DGNOME_DISABLE_SINGLE_INCLUDES',
  '-DGNOME_VFS_DISABLE_DEPRECATED',
  '-DGNOME_VFS_DISABLE_SINGLE_INCLUDES',
  '-DLIBGLADE_DISABLE_DEPRECATED',
  '-DLIBGLADE_DISABLE_SINGLE_INCLUDES',
  '-DLIBSOUP_DISABLE_DEPRECATED',
  '-DLIBSOUP_DISABLE_SINGLE_INCLUDES',
  '-DWNCK_DISABLE_DEPRECATED',
  '-DWNCK_DISABLE_SINGLE_INCLUDES'
]

compiler_flags = []
if gnomebt_debug
  test_flags = [
    '-Werror=format=2',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-prototypes',
    '-Werror=missing-include-dirs',
    '-Werror=pointer-arith',
    '-Werror=return-type',
    '-Wnested-externs',
    '-Wstrict-prototypes'
  ]

  foreach flag: test_flags
    if cc.has_argument(flag)
      compiler_flags += [flag]
    endif
  endforeach
endif

add_project_arguments(common_flags + compiler_flags, language: 'c')

gtk_dep = dependency('gtk+-3.0', version: '>= 3.12.0')

m_dep = cc.find_library('m')
config_h.set('HAVE_LIBM', m_dep.found() and cc.has_function('sin', dependencies: m_dep))

configure_file(
  output: 'config.h',
  configuration: config_h
)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

po_dir = join_paths(meson.source_root(), 'po')

top_inc = include_directories('.')

subdir('icons')
subdir('lib')
subdir('sendto')

if enable_gtk_doc
  subdir('docs/reference/libgnome-bluetooth')
endif

subdir('po')

meson.add_install_script('meson_post_install.py', get_option('enable-icon-update') ? 'icon-update' : '')

output = '\nConfigure summary:\n\n'
output += '        Compiler....................:  ' + cc.get_id() + '\n\n'
output += '        Compiler Flags..............:  ' + ' '.join(compiler_flags) + '\n'
output += '        Prefix......................:  ' + gnomebt_prefix + '\n'
output += '        Documentation...............:  ' + enable_gtk_doc.to_string() + '\n'
output += '        GObject-Introspection.......:  ' + enable_gir.to_string()
message(output)
