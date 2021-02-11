conf = configuration_data()
conf.set_quoted('VERSION', gclue_version)
version_file = configure_file(output: 'version.xml',
                              input: 'version.xml.in',
                              configuration: conf)

content_files = [ '../../COPYING.LIB' ]
scan_options = [ '--ignore-headers=config.h', '--rebuild-types' ]
# Extra options to supply to gtkdoc-mkdb
mkdb_options = [ '--output-format=xml' ]
src_dir = [ 'public-api', 'libgeoclue' ]

fixxref_options = [ '--extra-dir=../html' ]

gnome.gtkdoc('libgeoclue',
             main_xml: 'libgeoclue-docs.xml',
             content_files: content_files,
             gobject_typesfile: 'libgeoclue.types',
             dependencies: libgeoclue_dep,
             scan_args: scan_options,
             fixxref_args: fixxref_options,
             src_dir: src_dir,
             mkdb_args: mkdb_options,
             install: true)
