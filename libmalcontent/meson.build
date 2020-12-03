libmalcontent_api_version = '0'
libmalcontent_api_name = 'malcontent-' + libmalcontent_api_version
libmalcontent_sources = [
  'app-filter.c',
  'init.c',
  'manager.c',
  'session-limits.c',
]
libmalcontent_headers = [
  'app-filter.h',
  'malcontent.h',
  'manager.h',
  'session-limits.h',
]
libmalcontent_private_headers = [
  'app-filter-private.h',
  'gconstructor.h',
  'session-limits-private.h',
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

enums = gnome.mkenums_simple('enums',
  sources: libmalcontent_headers,
  install_header: true,
  install_dir: join_paths(includedir, libmalcontent_include_subdir),
)

libmalcontent = library(libmalcontent_api_name,
  libmalcontent_sources + libmalcontent_headers + libmalcontent_private_headers + enums,
  dependencies: libmalcontent_public_deps + libmalcontent_private_deps,
  include_directories: root_inc,
  install: true,
  version: meson.project_version(),
  soversion: libmalcontent_api_version,
)
libmalcontent_dep = declare_dependency(
  link_with: libmalcontent,
  include_directories: root_inc,
  sources: libmalcontent_headers + [enums[1]],
)

# Public library bits.
install_headers(libmalcontent_headers,
  subdir: libmalcontent_include_subdir,
)

pkgconfig.generate(libmalcontent,
  subdirs: libmalcontent_api_name,
  version: meson.project_version(),
  name: 'libmalcontent',
  filebase: libmalcontent_api_name,
  description: 'Library providing access to parental control settings.',
  libraries: libmalcontent_public_deps,
  libraries_private: libmalcontent_private_deps,
)

libmalcontent_gir = gnome.generate_gir(libmalcontent,
  sources: libmalcontent_sources + libmalcontent_headers + libmalcontent_private_headers + enums,
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