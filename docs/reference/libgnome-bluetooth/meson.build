private_headers = [
  'bluetooth-agent.h',
  'bluetooth-chooser-private.h',
  'bluetooth-client-glue.h',
  'bluetooth-client-private.h',
  'bluetooth-fdo-glue.h',
  'bluetooth-settings-obexpush.h',
  'bluetooth-settings-row.h',
  'gnome-bluetooth-enum-types.h'
]

version_conf = configuration_data()
version_conf.set('VERSION', gnomebt_version)

version_xml = 'version.xml'

configure_file(
  input: version_xml + '.in',
  output: version_xml,
  configuration: version_conf
)

doc_path = join_paths(gnomebt_datadir, 'gtk-doc', 'html', meson.project_name())

gnome.gtkdoc(
  meson.project_name(),
  main_sgml: meson.project_name() + '-docs.sgml',
  src_dir: [
    top_inc,
    lib_inc
  ],
  dependencies: libgnome_bluetooth_dep,
  scan_args: [
    '--rebuild-types',
    '--ignore-headers=' + ' '.join(private_headers),
  ],
  mkdb_args: [
    '--sgml-mode',
    '--output-format=xml'
  ],
  gobject_typesfile: meson.project_name() + '.types',
  install: true,
  install_dir: doc_path
)
