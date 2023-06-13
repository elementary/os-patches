libchamplain_public_h = [
  'champlain-adjustment.h',
  'champlain-bounding-box.h',
  'champlain-coordinate.h',
  'champlain-custom-marker.h',
  'champlain-defines.h',
  'champlain-error-tile-renderer.h',
  'champlain-exportable.h',
  'champlain-file-cache.h',
  'champlain-file-tile-source.h',
  'champlain-image-renderer.h',
  'champlain-kinetic-scroll-view.h',
  'champlain-label.h',
  'champlain-layer.h',
  'champlain-license.h',
  'champlain-location.h',
  'champlain-map-source-chain.h',
  'champlain-map-source-desc.h',
  'champlain-map-source-factory.h',
  'champlain-map-source.h',
  'champlain-marker-layer.h',
  'champlain-marker.h',
  'champlain-memory-cache.h',
  'champlain-network-bbox-tile-source.h',
  'champlain-network-tile-source.h',
  'champlain-null-tile-source.h',
  'champlain-path-layer.h',
  'champlain-point.h',
  'champlain-renderer.h',
  'champlain-scale.h',
  'champlain-tile-cache.h',
  'champlain-tile-source.h',
  'champlain-tile.h',
  'champlain-view.h',
  'champlain-viewport.h',
  'champlain.h',
]

if build_with_memphis
  libchamplain_public_h += [
    'champlain-memphis-renderer.h',
  ]
endif

libchamplain_sources = [
  'champlain-adjustment.c',
  'champlain-bounding-box.c',
  'champlain-coordinate.c',
  'champlain-custom-marker.c',
  'champlain-debug.c',
  'champlain-error-tile-renderer.c',
  'champlain-exportable.c',
  'champlain-file-cache.c',
  'champlain-file-tile-source.c',
  'champlain-image-renderer.c',
  'champlain-kinetic-scroll-view.c',
  'champlain-label.c',
  'champlain-layer.c',
  'champlain-license.c',
  'champlain-location.c',
  'champlain-map-source-chain.c',
  'champlain-map-source-desc.c',
  'champlain-map-source-factory.c',
  'champlain-map-source.c',
  'champlain-marker-layer.c',
  'champlain-marker.c',
  'champlain-memory-cache.c',
  'champlain-network-bbox-tile-source.c',
  'champlain-network-tile-source.c',
  'champlain-null-tile-source.c',
  'champlain-path-layer.c',
  'champlain-point.c',
  'champlain-renderer.c',
  'champlain-scale.c',
  'champlain-tile-cache.c',
  'champlain-tile-source.c',
  'champlain-tile.c',
  'champlain-view.c',
  'champlain-viewport.c',
]

if build_with_memphis
  libchamplain_sources += [
    'champlain-memphis-renderer.c',
  ]
endif

libchamplain_requires = [
  glib_dep,
  gobject_dep,
  cairo_dep,
  clutter_dep,
]

libchamplain_deps = libchamplain_requires + [
  libm_dep,
  gio_dep,
  gdk_dep,
  sqlite_dep,
  libsoup_dep,
]

if build_with_memphis
  libchamplain_deps += memphis_dep
endif

libchamplain_srcdir = include_directories('.')

libchamplain_c_args = [
  '-DHAVE_CONFIG_H',
  '-DCHAMPLAIN_COMPILATION',
  '-DG_LOG_DOMAIN="@0@"'.format(meson.project_name()),
]

if get_option('libsoup3')
  libchamplain_c_args += [
    '-DCHAMPLAIN_LIBSOUP_3',
  ]
endif

libchamplain_link_args = [
]

libchamplain_includedir = join_paths(
  pkgincludedir,
  'champlain',
)

features_h = configuration_data()
if build_with_memphis
  features_h.set('CHAMPLAIN_HAS_MEMPHIS', '1')
endif

libchamplain_features_h = configure_file(
  output: 'champlain-features.h',
  configuration: features_h,
)

version_h = configuration_data()
version_h.set('CHAMPLAIN_MAJOR_VERSION', version_major)
version_h.set('CHAMPLAIN_MINOR_VERSION', version_minor)
version_h.set('CHAMPLAIN_MICRO_VERSION', version_micro)
version_h.set('CHAMPLAIN_VERSION', version)

libchamplain_version_h = configure_file(
  input: 'champlain-version.h.in',
  output: 'champlain-version.h',
  configuration: version_h,
)

libchamplain_public_h += [
  libchamplain_features_h,
  libchamplain_version_h,
]

libchamplain_enums = gnome.mkenums(
  'champlain-enum-types',
  sources: libchamplain_public_h,
  h_template: 'champlain-enum-types.h.in',
  c_template: 'champlain-enum-types.c.in',
  install_header: true,
  install_dir: libchamplain_includedir,
)

libchamplain_enums_h = libchamplain_enums.get(1)

libchamplain_sources += [
  libchamplain_enums,
]

install_headers(
  libchamplain_public_h,
  install_dir: libchamplain_includedir,
)

libchamplain_sha = library(
  package_string,
  libchamplain_sources,
  version: lib_version,
  include_directories: rootdir,
  dependencies: libchamplain_deps,
  c_args: libchamplain_c_args,
  link_args: libchamplain_link_args,
  install: true,
)

libchamplain_dep_sources = [
  libchamplain_enums_h,
  libchamplain_features_h,
  libchamplain_version_h,
]

if generate_gir
  libchamplain_gir_includes = [
    'GObject-2.0',
    'Clutter-1.0',
  ]

  if build_with_memphis
    libchamplain_gir_includes += 'Memphis-0.2'
  endif

  libchamplain_gir = gnome.generate_gir(
    libchamplain_sha,
    sources: libchamplain_sources + libchamplain_public_h,
    nsversion: api_version,
    namespace: 'Champlain',
    symbol_prefix: 'champlain',
    identifier_prefix: 'Champlain',
    header: 'champlain/champlain.h',
    export_packages: [package_string],
    includes: libchamplain_gir_includes,
    link_with: libchamplain_sha,
    install: true,
    extra_args: [
      '-DCHAMPLAIN_COMPILATION',
    ]
  )

  libchamplain_dep_sources += [
    libchamplain_gir,
  ]

  if generate_vapi
    libchamplain_vapi_packages = [
      'clutter-1.0',
      'cogl-pango-1.0',
      'atk',
      'pangocairo',
    ]

    libchamplain_vapi = gnome.generate_vapi(
      package_string,
      sources: libchamplain_gir.get(0),
      packages: libchamplain_vapi_packages,
      install: true,
    )
  endif
endif

libchamplain_dep = declare_dependency(
  link_with: libchamplain_sha,
  include_directories: rootdir,
  dependencies: libchamplain_requires,
  sources: libchamplain_dep_sources,
)

libchamplain_pc = pkg.generate(
  libchamplain_sha,
  description: 'Map View for Clutter',
  subdirs: package_string,
  requires: libchamplain_requires,
  variables: [
    'soupapiversion=@0@'.format(libsoup_api_version),
  ],
)
