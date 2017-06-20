libgtkcloudproviders_headers = [
  'gtkcloudprovidermanager.h',
  'gtkcloudprovider.h',
  'cloudprovider-generated.h',
  'cloudprovidermanager-generated.h'
]

libgtkcloudproviders_sources = [
  'gtkcloudprovidermanager.c',
  'gtkcloudprovider.c'
]

libgtkcloudproviders_deps = [glib, gio, gio_unix]

gdbus_sources = []
gdbus_sources += gnome.gdbus_codegen ('cloudprovider-generated',
                                      join_paths (meson.source_root (), 'data', 'cloudprovider-dbus.xml'),
                                      interface_prefix: 'org.freedesktop',
                                      namespace: '')
gdbus_sources += gnome.gdbus_codegen ('cloudprovidermanager-generated',
                                      join_paths (meson.source_root (), 'data', 'cloudprovidermanager-dbus.xml'),
                                      interface_prefix: 'org.freedesktop',
                                      namespace: '')

libgtkcloudproviders_sources += gdbus_sources

libgtkcloudproviders = shared_library ('cloudproviders',
                                        libgtkcloudproviders_sources,
                                        dependencies: libgtkcloudproviders_deps,
                                        version: meson.project_version (),
                                        install: true)

install_headers( libgtkcloudproviders_headers, subdir: 'cloudproviders')

pkg = import('pkgconfig')
pkg.generate(libraries : libgtkcloudproviders,
             subdirs : 'cloudproviders',
             version : '1.0',
             name : 'libcloudproviders',
             filebase: 'cloudproviders',
             description : 'Cloud providers DBus API library.')
