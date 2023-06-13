glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html', 'glib')

gobject_prefix = gobject_dep.get_pkgconfig_variable('prefix')
gobject_docpath = join_paths(gobject_prefix, 'share', 'gtk-doc', 'html', 'gobject')

clutter_prefix = clutter_dep.get_pkgconfig_variable('prefix')
clutter_docpath = join_paths(clutter_prefix, 'share', 'gtk-doc', 'html', 'clutter')

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_docpath = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html', 'gtk3')

gtkdocdir = join_paths(datadir, 'gtk-doc', 'html', package_string)

libchamplain_reference_ignored_h = [
  'champlain-gtk.h',
  'champlain-adjustment.h',
  'champlain-debug.h',
  'champlain-defines.h',
  'champlain-enum-types.h',
  'champlain-features.h',
  'champlain-kinetic-scroll-view.h',
  'champlain-private.h',
  'champlain-viewport.h',
  'champlain.h',
]

if not build_with_memphis
  libchamplain_reference_ignored_h += [
    'champlain-memphis-renderer.h',
  ]
endif

version_xml = configuration_data()
version_xml.set('PACKAGE_VERSION', version)

configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: version_xml,
  install: false,
)

libchamplain_reference_scan_args = [
  '--rebuild-types',
  '--deprecated-guards=GTK_DISABLE_DEPRECATED',
  '--ignore-headers=' + ' '.join(libchamplain_reference_ignored_h),
]

libchamplain_reference_mkdb_args = [
  '--output-format=xml',
  '--name-space=champlain',
  '--name-space=gtk_champlain',
]

libchamplain_reference_fixxref_args = [
  '--html-dir=@0@'.format(gtkdocdir),
  '--extra-dir=@0@'.format(glib_docpath),
  '--extra-dir=@0@'.format(gobject_docpath),
  '--extra-dir=@0@'.format(clutter_docpath),
]

gnome.gtkdoc(
  package_string,
  main_xml: 'champlain-docs.xml',
  mode: 'xml',
  gobject_typesfile: files('champlain.types'),
  src_dir: [libchamplain_srcdir, libchamplain_gtk_srcdir],
  dependencies: [libchamplain_dep, libchamplain_gtk_dep],
  scan_args: libchamplain_reference_scan_args,
  mkdb_args: libchamplain_reference_mkdb_args,
  fixxref_args: libchamplain_reference_fixxref_args,
  install: true,
  install_dir: gtkdocdir,
)
