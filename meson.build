project('malcontent', 'c',
  version : '0.4.0',
  meson_version : '>= 0.47.0',
  license: 'LGPLv2.1+',
  default_options : [
    'buildtype=debugoptimized',
    'warning_level=2',
    'c_std=gnu11',
  ]
)

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')

meson_make_symlink = join_paths(meson.source_root(), 'tools', 'meson-make-symlink.sh')
po_dir = join_paths(meson.source_root(), 'po')

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))

# FIXME: This isn’t exposed in accountsservice.pc
# See https://gitlab.freedesktop.org/accountsservice/accountsservice/merge_requests/16
accountsserviceinterfacesdir = join_paths(datadir, 'accountsservice', 'interfaces')

dbus = dependency('dbus-1')
dbusinterfacesdir = dbus.get_pkgconfig_variable('interfaces_dir',
  define_variable: ['datadir', datadir])

polkit_gobject = dependency('polkit-gobject-1')
polkitpolicydir = polkit_gobject.get_pkgconfig_variable('policydir',
  define_variable: ['prefix', prefix])

libglib_testing = subproject('libglib-testing')
libglib_testing_dep = dependency(
  'libglib-testing',
  fallback: ['libglib-testing', 'libglib_testing_dep'],
)

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name())
configure_file(
  output: 'config.h',
  configuration: config_h,
)
root_inc = include_directories('.')

# Enable warning flags
test_c_args = [
  '-fno-strict-aliasing',
  '-fstack-protector-strong',
  '-Waggregate-return',
  '-Wall',
  '-Wunused',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wno-declaration-after-statement',
  '-Wdiscarded-qualifiers',
  '-Wduplicated-branches',
  '-Wduplicated-cond',
  '-Wempty-body',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wincompatible-pointer-types',
  '-Wincompatible-pointer-types-discards-qualifiers',
  '-Winit-self',
  '-Wint-conversion',
  '-Wlogical-op',
  '-Wmisleading-indentation',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-error=cpp',
  '-Wmissing-field-initializers',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wnull-dereference',
  '-Wold-style-definition',
  '-Woverflow',
  '-Woverride-init',
  '-Wparentheses',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing=2',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wunused-result',
  '-Wunused-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(test_c_args), language: 'c')

enable_installed_tests = get_option('installed_tests')
test_template = files('template.test.in')
test_env = [
  'G_DEBUG=gc-friendly,fatal-warnings',
  'MALLOC_CHECK_=2',
  'LC_ALL=C.UTF-8',
]

subdir('accounts-service')
subdir('malcontent-client')
subdir('libmalcontent')