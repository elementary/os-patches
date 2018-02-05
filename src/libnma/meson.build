lib_name = 'libnma'

cert_chooser_headers = files('nma-cert-chooser.h')

cert_chooser_sources = files(
  'nma-cert-chooser.c',
  'nma-file-cert-chooser.c'
)

resource_data = files(
  'nma-pkcs11-cert-chooser-dialog.ui',
  'nma-pkcs11-token-login-dialog.ui',
  'wifi.ui'
)

built_sources = gnome.compile_resources(
  'nma-resources',
  'nma.gresource.xml',
  dependencies: resource_data
)

gir_headers = cert_chooser_headers + files(
  'nma-mobile-providers.h',
  'nma-mobile-wizard.h',
  'nma-ui-utils.h',
  'nma-vpn-password-dialog.h',
  'nma-wifi-dialog.h'
)

headers = gir_headers + files('nma-cert-chooser-private.h')

install_headers(
  gir_headers,
  subdir: lib_name
)

gir_sources = [version_header] + cert_chooser_sources + files(
  'init.c',
  'nma-mobile-providers.c',
  'nma-mobile-wizard.c',
  'nma-ui-utils.c',
  'nma-vpn-password-dialog.c',
  'nma-wifi-dialog.c'
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
  libnm_dep,
  libutils_libnm_dep,
]

cflags = [
  '-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB',
  '-DBINDIR="@0@"'.format(nma_bindir),
  '-DDATADIR="@0@"'.format(nma_datadir),
  '-DLIBEXECDIR="@0@"'.format(nma_libexecdir),
  '-DNMALOCALEDIR="@0@"'.format(nma_localedir),
  '-DSYSCONFDIR="@0@"'.format(nma_sysconfdir),
  '-DAUTOSTARTDIR="@0@"'.format(nma_autostartdir),
  '-DICONDIR="@0@"'.format(nma_icondir)
]

symbol_map = join_paths(meson.current_source_dir(), 'libnma.ver')

ldflags = []
if have_version_script
  ldflags += '-Wl,--version-script,' + symbol_map
endif

if enable_gcr
  headers += files(
    'nma-cert-chooser-button.h',
    'nma-pkcs11-cert-chooser-dialog.h',
    'nma-pkcs11-token-login-dialog.h'
  )

  gir_sources += files(
    'nma-cert-chooser-button.c',
    'nma-pkcs11-cert-chooser.c',
    'nma-pkcs11-cert-chooser-dialog.c',
    'nma-pkcs11-token-login-dialog.c'
  )

  deps += gcr_dep
endif

libnma = shared_library(
  'nma',
  sources: gir_sources + built_sources,
  version: libversion,
  include_directories: incs,
  dependencies: deps,
  c_args: cflags,
  link_args: ldflags,
  link_depends: symbol_map,
  link_whole: libwireless_security_libnm,
  install: true,
  install_dir: nma_libdir
)

libnma_dep = declare_dependency(
  link_with: libnma,
  include_directories: libnma_inc
)

pkg.generate(
  libraries: libnma,
  version: nma_version,
  name: lib_name,
  description: 'NetworkManager UI utilities (libnm version)',
  filebase: lib_name,
  subdirs: lib_name,
  requires: 'libnm',
  variables: 'exec_prefix=' + nma_prefix,
  install_dir: join_paths(nma_libdir, 'pkgconfig')
)

if enable_introspection
  gir_incs = [
    'Gtk-3.0',
    'NM-1.0'
  ]

  gir_dir = join_paths(nma_datadir, 'gir-' + nma_gir_version)
  typelib_dir = join_paths(nma_libdir, 'girepository-' + nma_gir_version)

  gnome.generate_gir(
    libnma,
    sources: gir_sources + gir_headers,
    nsversion: nma_gir_version,
    namespace: nma_gir_ns,
    includes: gir_incs,
    identifier_prefix: nma_gir_prefix,
    symbol_prefix: nma_gir_prefix.to_lower(),
    export_packages: lib_name,
    install: true,
    install_dir_gir: gir_dir,
    install_dir_typelib: typelib_dir
  )
endif
