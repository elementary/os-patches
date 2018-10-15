libcloudproviders_deps = [glib, gio, gio_unix]

libcloudproviders_headers = [
  'cloudproviders.h',
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
                                          install_dir: incdir,
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

libcloudproviders_dep = declare_dependency (
  link_with: libcloudproviders,
  dependencies: libcloudproviders_deps,
  include_directories: include_directories('.')
)

if get_option('introspection')
  libcloudproviders_gir = gnome.generate_gir(
    libcloudproviders,
    sources: libcloudproviders_headers + libcloudproviders_sources,
    nsversion: meson.project_version(),
    dependencies: libcloudproviders_deps,
    namespace: 'CloudProviders',
    symbol_prefix: 'cloud_providers',
    link_with: libcloudproviders,
    includes: [ 'GLib-2.0', 'Gio-2.0' ],
    install: true,
    install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
    extra_args: [ '--c-include=cloudproviders/cloudproviders.h', '--quiet' ]
  )
endif

if get_option('vapigen')
  libcloudproviders_vapi = gnome.generate_vapi(
    'cloudproviders',
    sources: libcloudproviders_gir[0],
    packages: ['gio-2.0'],
    install: true,
  )
endif