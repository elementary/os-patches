libchamplain_gtk_public_h = [
  'champlain-gtk.h',
  'gtk-champlain-embed.h',
]

libchamplain_gtk_sources = [
  'gtk-champlain-embed.c',
]

libchamplain_gtk_requires = [
  gtk_dep,
  clutter_gtk_dep,
]

libchamplain_gtk_deps = libchamplain_gtk_requires + [
  glib_dep,
  gobject_dep,
  libchamplain_dep,
]

libchamplain_gtk_srcdir = include_directories('.')

libchamplain_gtk_c_args = [
  '-DHAVE_CONFIG_H',
  '-DCHAMPLAIN_GTK_COMPILATION',
  '-DG_LOG_DOMAIN="@0@"'.format(meson.project_name()),
]

libchamplain_gtk_link_args = [
]

libchamplain_gtk_includedir = join_paths(
  pkgincludedir,
  'champlain-gtk',
)

install_headers(
  libchamplain_gtk_public_h,
  install_dir: libchamplain_gtk_includedir,
)

libchamplain_gtk_sha = library(
  package_gtk_string,
  libchamplain_gtk_sources,
  version: lib_version,
  include_directories: rootdir,
  dependencies: libchamplain_gtk_deps,
  c_args: libchamplain_gtk_c_args,
  link_args: libchamplain_gtk_link_args,
  install: true,
)

libchamplain_gtk_dep_sources = [
]

if generate_gir
  libchamplain_gtk_gir_includes = [
    'GObject-2.0',
    'Clutter-1.0',
    'Gtk-3.0',
    libchamplain_gir.get(0),
  ]

  libchamplain_gtk_gir = gnome.generate_gir(
    libchamplain_gtk_sha,
    sources: libchamplain_gtk_sources + libchamplain_gtk_public_h,
    nsversion: api_version,
    namespace: 'GtkChamplain',
    symbol_prefix: 'gtk_champlain',
    identifier_prefix: 'GtkChamplain',
    header: 'champlain-gtk/champlain-gtk.h',
    export_packages: [package_gtk_string],
    includes: ['Clutter-1.0', 'Gtk-3.0', libchamplain_gir.get(0)],
    link_with: libchamplain_gtk_sha,
    install: true,
    extra_args: [
      '-DCHAMPLAIN_GTK_COMPILATION',
    ]
  )

  libchamplain_gtk_dep_sources += [
    libchamplain_gtk_gir,
  ]

  if generate_vapi
    libchamplain_gtk_vapi_packages = [
      'clutter-gtk-1.0',
      'cogl-pango-1.0',
      'gtk+-3.0',
      'atk',
      'pangocairo',
      libchamplain_vapi,
    ]

    libchamplain_gtk_vapi = gnome.generate_vapi(
      package_gtk_string,
      sources: libchamplain_gtk_gir.get(0),
      packages: libchamplain_gtk_vapi_packages,
      install: true,
    )
  endif
endif

libchamplain_gtk_dep = declare_dependency(
  link_with: libchamplain_gtk_sha,
  include_directories: rootdir,
  dependencies: libchamplain_gtk_requires + [libchamplain_dep],
  sources: libchamplain_gtk_dep_sources,
)

libchamplain_gtk_pc = pkg.generate(
  libchamplain_gtk_sha,
  description: 'Gtk+ Widget wrapper for libchamplain',
  subdirs: package_string,
  requires: libchamplain_gtk_requires + [libchamplain_sha],
)
