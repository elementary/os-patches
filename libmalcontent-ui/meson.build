libmalcontent_ui_api_version = '0'
libmalcontent_ui_api_name = 'malcontent-ui-' + libmalcontent_ui_api_version

if not cc.has_function('atexit')
  error('atexit() needed for generated GResource files')
endif

resources = gnome.compile_resources(
  'resources',
  'malcontent-ui.gresource.xml',
  source_dir: meson.current_source_dir(),
)

libmalcontent_ui_sources = [
  'gs-content-rating.c',
  'restrict-applications-dialog.c',
  'restrict-applications-selector.c',
  'user-controls.c',
] + resources
libmalcontent_ui_headers = [
  'malcontent-ui.h',
  'restrict-applications-dialog.h',
  'restrict-applications-selector.h',
  'user-controls.h',
]
libmalcontent_ui_private_headers = [
  'gs-content-rating.h',
]

libmalcontent_ui_public_deps = [
  dependency('accountsservice', version: '>= 0.6.39'),
  dependency('gio-2.0', version: '>= 2.44'),
  dependency('glib-2.0', version: '>= 2.54.2'),
  dependency('gobject-2.0', version: '>= 2.54'),
  dependency('gtk+-3.0', version: '>= 3.24'),
  libmalcontent_dep,
]
libmalcontent_ui_private_deps = [
  dependency('appstream-glib', version: '>= 0.7.15'),
  dependency('flatpak'),
]

# FIXME: Would be good to use subdir here: https://github.com/mesonbuild/meson/issues/2969
libmalcontent_ui_include_subdir = join_paths(libmalcontent_ui_api_name, 'libmalcontent-ui')

libmalcontent_ui = library(libmalcontent_ui_api_name,
  libmalcontent_ui_sources + libmalcontent_ui_headers + libmalcontent_ui_private_headers,
  dependencies: libmalcontent_ui_public_deps + libmalcontent_ui_private_deps,
  include_directories: root_inc,
  install: true,
  version: meson.project_version(),
  soversion: libmalcontent_ui_api_version,
)
libmalcontent_ui_dep = declare_dependency(
  link_with: libmalcontent_ui,
  include_directories: root_inc,
)

# Public library bits.
install_headers(libmalcontent_ui_headers,
  subdir: libmalcontent_ui_include_subdir,
)

pkgconfig.generate(libmalcontent_ui,
  subdirs: libmalcontent_ui_api_name,
  version: meson.project_version(),
  name: 'libmalcontent-ui',
  filebase: libmalcontent_ui_api_name,
  description: 'Library providing widgets for parental control settings.',
  libraries: libmalcontent_ui_public_deps,
  libraries_private: libmalcontent_ui_private_deps,
)

libmalcontent_ui_gir = gnome.generate_gir(libmalcontent_ui,
  sources: libmalcontent_ui_sources + libmalcontent_ui_headers + libmalcontent_ui_private_headers,
  nsversion: libmalcontent_ui_api_version,
  namespace: 'MalcontentUi',
  symbol_prefix: 'mct_',
  identifier_prefix: 'Mct',
  export_packages: 'libmalcontent-ui',
  includes: ['AccountsService-1.0', 'Gio-2.0', 'GObject-2.0', 'Gtk-3.0', libmalcontent_gir[0]],
  install: true,
  dependencies: libmalcontent_ui_dep,
)

xmllint = find_program('xmllint', required: false)
if xmllint.found()
  gtk_prefix = dependency('gtk+-3.0').get_pkgconfig_variable('prefix')
  test(
    'validate-ui', xmllint,
    args: [
      '--nonet', '--noblanks', '--noout',
      '--relaxng', join_paths(gtk_prefix, 'share', 'gtk-3.0', 'gtkbuilder.rng'),
      files(
        'restrict-applications-dialog.ui',
        'restrict-applications-selector.ui',
        'user-controls.ui',
      ),
    ],
    suite: ['libmalcontent-ui'],
  )
endif

# FIXME: Add tests
#subdir('tests')
