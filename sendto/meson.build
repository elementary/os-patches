name = 'bluetooth-sendto'

deps = [
  gtk_dep,
  dependency('gio-2.0')
]

executable(
  name,
  'main.c',
  include_directories: [
    top_inc,
    lib_inc
  ],
  dependencies: deps,
  link_with: libgnome_bluetooth,
  install: true,
  install_dir: gnomebt_bindir
)

install_man(
  name + '.1',
  install_dir: join_paths(gnomebt_mandir, 'man1')
)

desktop_conf = configuration_data()
desktop_conf.set('VERSION', gnomebt_version)

desktop = name + '.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file (
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(gnomebt_datadir, 'applications')
)
