libcloudproviders_headers = [
  'cloudprovidermanager.h',
  'cloudproviderproxy.h',
  'cloudproviders.h',
  'cloudprovider.h',
  'cloudprovideraccount.h',
]

libcloudproviders_sources = [
  'cloudprovidermanager.c',
  'cloudproviderproxy.c',
  'cloudproviders.c',
  'cloudprovider.c',
  'cloudprovideraccount.c',
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

install_headers (libcloudproviders_headers, subdir: 'cloudproviders')

pkg = import('pkgconfig')
pkg.generate(libraries : libcloudproviders,
             subdirs : 'cloudproviders',
             version : meson.project_version(),
             name : 'libcloudproviders',
             filebase: 'cloudproviders',
             description : 'Cloud providers DBus API library.')

libdir = join_paths(get_option('prefix'),get_option('libdir'))
libdir = get_option('libdir')
libexecdir = get_option ('libexecdir')
unitdir = 'lib/systemd/user'
service_conf = configuration_data()
service_conf.set('libexecdir', libexecdir)
configure_file(input: 'libcloudproviders.service.in',
               output: 'libcloudproviders.service',
               configuration: service_conf,
               install_dir: unitdir)


cloudproviderd_deps = declare_dependency (link_with: libcloudproviders,
                                          include_directories: include_directories('.'),
                                          dependencies: libcloudproviders_deps)

executable('cloudproviderd', 'cloudproviderd.c',
           dependencies: cloudproviderd_deps,
           install: true,
           install_dir: libexecdir)
