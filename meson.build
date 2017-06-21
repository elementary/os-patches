project ('libcloudproviders', 'c',
         version: '1.0.0',
         meson_version: '>=0.40.0',
         license: 'GPL3+')

add_global_arguments ('-Wno-deprecated-declarations', language:'c')

glib_ver = '>=2.51.2'
glib = dependency ('glib-2.0', version: glib_ver)
gio = dependency ('gio-2.0', version: glib_ver)
gio_unix = dependency ('gio-unix-2.0', version: glib_ver)

prefix = get_option ('prefix')
libdir = get_option ('libdir')
bindir = get_option ('bindir')
datadir = get_option ('datadir')
servicedir = join_paths (datadir, 'dbus-1', 'services')

gnome = import('gnome')

conf = configuration_data ()
conf.set_quoted ('VERSION', meson.project_version ())
conf.set_quoted ('PACKAGE_VERSION', meson.project_version ())
conf.set_quoted ('GETTEXT_PACKAGE', 'cloudproviders')

subdir ( 'src' )
subdir ( 'test' )
