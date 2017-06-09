libgtkcloudproviders_headers = [
  'gtkcloudprovidermanager.h',
  'gtkcloudprovider.h'
]

libgtkcloudproviders_sources = [
  'gtkcloudprovidermanager.c',
  'gtkcloudprovider.c'
]

libgtkcloudproviders_deps = [glib, gio, gio_unix]

gdbus_sources = []
gdbus_sources += gnome.gdbus_codegen ('gtkcloudprovider-generated',
                                      join_paths (meson.source_root (), 'data', 'cloudprovider-dbus.xml'),
                                      interface_prefix: 'org.freedesktop',
                                      namespace: 'Gtk')
gdbus_sources += gnome.gdbus_codegen ('gtkcloudprovidermanager-generated',
                                      join_paths (meson.source_root (), 'data', 'cloudprovidermanager-dbus.xml'),
                                      interface_prefix: 'org.freedesktop',
                                      namespace: 'Gtk')

libgtkcloudproviders_sources += gdbus_sources

libgtkcloudproviders = shared_library ('gtkcloudproviders',
                                        libgtkcloudproviders_sources,
                                        dependencies: libgtkcloudproviders_deps,
                                        version: meson.project_version (),
                                        install: true)

install_headers( libgtkcloudproviders_headers )

