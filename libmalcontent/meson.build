libmalcontent_api_version = '0'
libmalcontent_api_name = 'malcontent-' + libmalcontent_api_version
libmalcontent_sources = [
  'app-filter.c',
  'manager.c',
]
libmalcontent_headers = [
  'app-filter.h',
  'malcontent.h',
  'manager.h',
]
libmalcontent_private_headers = [
  'app-filter-private.h',
]

libmalcontent_public_deps = [
  dependency('gio-2.0', version: '>= 2.44'),
  dependency('glib-2.0', version: '>= 2.54.2'),
  dependency('gobject-2.0', version: '>= 2.54'),
]
libmalcontent_private_deps = [
  dependency('gio-unix-2.0', version: '>= 2.36'),
]

# FIXME: Would be good to use subdir here: https://github.com/mesonbuild/meson/issues/2969
libmalcontent_include_subdir = join_paths(libmalcontent_api_name, 'libmalcontent')

libmalcontent = library(libmalcontent_api_name,
  libmalcontent_sources + libmalcontent_headers + libmalcontent_private_headers,
  dependencies: libmalcontent_public_deps + libmalcontent_private_deps,
  include_directories: root_inc,
  install: true,
  version: meson.project_version(),
  soversion: libmalcontent_api_version,
)
libmalcontent_dep = declare_dependency(
  link_with: libmalcontent,
  include_directories: root_inc,
)

# Public library bits.
install_headers(libmalcontent_headers,
  subdir: libmalcontent_include_subdir,
)

pkgconfig.generate(
  libraries: [ libmalcontent ],
  subdirs: libmalcontent_api_name,
  version: meson.project_version(),
  name: 'libmalcontent',
  filebase: libmalcontent_api_name,
  description: 'Library providing access to parental control settings.',
  requires: libmalcontent_public_deps,
  requires_private: libmalcontent_private_deps,
)

gnome.generate_gir(libmalcontent,
  sources: libmalcontent_sources + libmalcontent_headers + libmalcontent_private_headers,
  nsversion: libmalcontent_api_version,
  namespace: 'Malcontent',
  symbol_prefix: 'mct_',
  identifier_prefix: 'Mct',
  export_packages: 'libmalcontent',
  includes: ['GObject-2.0', 'Gio-2.0'],
  install: true,
  dependencies: libmalcontent_dep,
)

subdir('tests')