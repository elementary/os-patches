lib_name = 'libnm-gtk'

gir_headers = files(
  'nm-cert-chooser.h',
  'nm-mobile-providers.h',
  'nm-mobile-wizard.h',
  'nm-ui-utils.h',
  'nm-vpn-password-dialog.h',
  'nm-wifi-dialog.h'
)

headers = gir_headers + files('nm-wireless-dialog.h')

install_headers(
  headers,
  subdir: lib_name
)

gir_sources = cert_chooser_sources + files(
  'init.c',
  'nm-mobile-providers.c',
  'nm-mobile-wizard.c',
  'nm-ui-utils.c',
  'nm-vpn-password-dialog.c',
  'nm-wifi-dialog.c'
)

sources = [version_header] + gir_sources + files('nm-wireless-dialog.c')

resource_data = files('wifi.ui')

sources += gnome.compile_resources(
  'nm-resources',
  'nm.gresource.xml',
  dependencies: resource_data
)

incs = [
  top_inc,
  shared_inc,
  src_inc,
  libnma_inc,
  wireless_security_inc
]

deps = [
  gtk_dep,
  gudev_dep,
  libnm_glib_dep,
  libutils_libnm_glib_dep,
]

cflags = [
  '-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_LEGACY',
  '-DBINDIR="@0@"'.format(nma_bindir),
  '-DDATADIR="@0@"'.format(nma_datadir),
  '-DLIBEXECDIR="@0@"'.format(nma_libexecdir),
  '-DNMALOCALEDIR="@0@"'.format(nma_localedir),
  '-DSYSCONFDIR="@0@"'.format(nma_sysconfdir),
  '-DAUTOSTARTDIR="@0@"'.format(nma_autostartdir),
  '-DICONDIR="@0@"'.format(nma_icondir)
]

symbol_map = join_paths(meson.current_source_dir(), 'libnm-gtk.ver')

ldflags = []
if have_version_script
  ldflags += '-Wl,--version-script,' + symbol_map
endif

libnm_gtk = shared_library(
  'nm-gtk',
  sources: sources,
  version: libversion,
  include_directories: incs,
  dependencies: deps,
  c_args: cflags,
  link_args: ldflags,
  link_depends: symbol_map,
  link_whole: libwireless_security_libnm_glib,
  install: true,
  install_dir: nma_libdir
)

pkg.generate(
  libraries: libnm_gtk,
  version: nma_version,
  name: lib_name,
  description: 'NetworkManager UI utilities (libnm-glib version)',
  filebase: lib_name,
  subdirs: lib_name,
  requires: [
    'NetworkManager ' + nm_req_version,
    'libnm-util ' + nm_req_version,
    'libnm-glib ' + nm_req_version,
    'gtk+-3.0 ' + gtk_req_version
  ],
  variables: 'exec_prefix=' + nma_prefix,
  install_dir: join_paths(nma_libdir, 'pkgconfig')
)

if enable_introspection
  gir_incs = [
    'Gtk-3.0',
    'NetworkManager-1.0',
    'NMClient-1.0'
  ]

  gir_dir = join_paths(nma_datadir, 'gir-' + nma_gir_version)
  typelib_dir = join_paths(nma_libdir, 'girepository-' + nma_gir_version)

  # FIXME: the generated gir file is slightly different
  gnome.generate_gir(
    libnm_gtk,
    sources: gir_sources + gir_headers,
    nsversion: nma_gir_version,
    namespace: nma_gtk_gir_ns,
    includes: gir_incs,
    identifier_prefix: nma_gir_prefix,
    symbol_prefix: nma_gir_prefix.to_lower(),
    export_packages: lib_name,
    install: true,
    install_dir_gir: gir_dir,
    install_dir_typelib: typelib_dir
  )
endif

test_unit = 'test-mobile-providers'

deps = [
 gtk_dep,
 libnm_glib_dep
]

cflags = '-DTEST_DATA_DIR="@0@"'.format(join_paths(meson.current_source_dir(), 'tests'))

exe = executable(
  test_unit,
  'tests/' + test_unit + '.c',
  include_directories: incs,
  dependencies: deps,
  c_args: cflags,
  link_with: libnm_gtk
)

test(test_unit, exe)

run_target(
  'check-local-exports-libnm-gtk',
  command: [find_program('check-exports.sh'), libnm_gtk.full_path(), symbol_map],
  depends: libnm_gtk
)
