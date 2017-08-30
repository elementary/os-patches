libcloudproviders_headers = [
  'cloudprovidermanager.h',
  'cloudproviderproxy.h',
  'cloudproviders.h',
  'cloudprovider.h',
]

libcloudproviders_sources = [
  'cloudprovidermanager.c',
  'cloudproviderproxy.c',
  'cloudproviders.c',
  'cloudprovider.c',
]

libcloudproviders_deps = [glib, gio, gio_unix]

generated_sources = []
generated_sources += gnome.gdbus_codegen ('cloudprovider-generated',
                                      join_paths (meson.source_root (), 'data', 'cloudprovider-dbus.xml'),
                                      interface_prefix: 'org.freedesktop.CloudProvider',
                                      namespace: 'CloudProvider',
                                      object_manager: true)
generated_sources += gnome.gdbus_codegen ('cloudprovidermanager-generated',
                                      join_paths (meson.source_root (), 'data', 'cloudprovidermanager-dbus.xml'),
                                      interface_prefix: 'org.freedesktop.CloudProvider',
                                      namespace: 'CloudProvider')

libcloudproviders_sources += generated_sources

libcloudproviders = shared_library ('cloudproviders',
                                    libcloudproviders_sources,
                                    dependencies: libcloudproviders_deps,
                                    version: meson.project_version (),
                                    install: true)

install_headers ( libcloudproviders_headers, subdir: 'cloudproviders')

pkg = import('pkgconfig')
pkg.generate(libraries : libcloudproviders,
             subdirs : 'cloudproviders',
             version : meson.project_version(),
             name : 'libcloudproviders',
             filebase: 'cloudproviders',
             description : 'Cloud providers DBus API library.')


cloudproviderd_deps = declare_dependency (link_with: libcloudproviders,
                                          include_directories: include_directories('.'),
                                          dependencies: libcloudproviders_deps)

executable('cloudproviderd', 'cloudproviderd.c',
           dependencies: cloudproviderd_deps, install: true)
