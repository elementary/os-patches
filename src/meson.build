libcloudproviders_deps = [glib, gio, gio_unix]

libcloudproviders_headers = [
  'cloudproviderscollector.h',
  'cloudprovidersaccount.h',
  'cloudprovidersaccountexporter.h',
  'cloudprovidersprovider.h',
  'cloudprovidersproviderexporter.h',
]

libcloudproviders_sources = [
  'cloudproviderscollector.c',
  'cloudprovidersaccount.c',
  'cloudprovidersaccountexporter.c',
  'cloudprovidersprovider.c',
  'cloudprovidersproviderexporter.c',
]

headers_with_enums = gnome.mkenums_simple('enums',
                                          sources : libcloudproviders_headers,
                                          install_header: true)

generated_sources = []
generated_sources += gnome.gdbus_codegen(
  'cloudproviders-generated', '../data/cloud-providers-dbus.xml',
  interface_prefix: 'org.freedesktop.CloudProviders',
  namespace: 'CloudProvidersDbus',
  object_manager: true
)

libcloudproviders_sources += generated_sources

libcloudproviders = shared_library ('cloudproviders',
  libcloudproviders_sources, headers_with_enums,
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

