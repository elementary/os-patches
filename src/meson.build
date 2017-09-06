libcloudproviders_headers = [
  'cloudproviders.h',
  'cloudprovideraccount.h',
  'cloudproviderexporter.h',
  'cloudprovideraccountexporter.h',
]

libcloudproviders_sources = [
  'cloudproviders.c',
  'cloudprovideraccount.c',
  'cloudproviderexporter.c',
  'cloudprovideraccountexporter.c',
]

cloudprovidersd_sources = [
  'cloudprovidersd.c',
  'cloudprovidermanager.c',
]

libcloudproviders_deps = [glib, gio, gio_unix]

generated_sources = []
generated_sources += gnome.gdbus_codegen(
  'cloudprovider-generated', '../data/cloudprovider-dbus.xml',
  interface_prefix: 'org.freedesktop.CloudProvider',
  namespace: 'CloudProvider',
  object_manager: true
)

generated_sources += gnome.gdbus_codegen(
  'cloudprovidermanager-generated', '../data/cloudprovidermanager-dbus.xml',
  interface_prefix: 'org.freedesktop.CloudProvider',
  namespace: 'CloudProvider'
)

libcloudproviders_sources += generated_sources

libcloudproviders = shared_library ('cloudproviders',
  libcloudproviders_sources,
  dependencies: libcloudproviders_deps,
  version: meson.project_version(),
  install: true
)

install_headers (libcloudproviders_headers, subdir: 'cloudproviders')

pkg = import('pkgconfig')
pkg.generate(
  libraries : libcloudproviders,
  subdirs : 'cloudproviders',
  version : meson.project_version(),
  name : 'libcloudproviders',
  filebase: 'cloudproviders',
  description : 'Cloud providers DBus API library.'
)

unitdir = join_paths(libdir, 'systemd/user')
service_conf = configuration_data()
service_conf.set('libexecdir', join_paths(prefix, libexecdir))
configure_file(
  input: 'libcloudproviders.service.in',
  output: 'libcloudproviders.service',
  configuration: service_conf,
  install_dir: unitdir
)


libcloudproviders_dep = declare_dependency (
  link_with: libcloudproviders,
  dependencies: libcloudproviders_deps,
  include_directories: include_directories('.')
)

executable('cloudprovidersd',
  cloudprovidersd_sources,
  dependencies: libcloudproviders_dep,
  install: true,
  install_dir: libexecdir
)
