if get_option('enable-backend')
  conf = configuration_data()
  conf.set_quoted('VERSION', gclue_version)
  version_file = configure_file(output: 'version.xml',
                                input: 'version.xml.in',
                                configuration: conf)

  content_files = [ '../COPYING',
                    geoclue_iface_sources ]
  scan_options = [ '--ignore-headers=config.h',
                   '--rebuild-sections',
                   '--rebuild-types' ]
  # Extra options to supply to gtkdoc-mkdb
  mkdb_options = [ '--output-format=xml' ]

  gnome.gtkdoc('geoclue',
               main_xml: 'geoclue-docs.xml',
               content_files: content_files,
               scan_args: scan_options,
               src_dir: libgeoclue_public_api_inc,
               mkdb_args: mkdb_options,
               dependencies: libgeoclue_dep,
               install: true)
endif

if get_option('libgeoclue')
    subdir('lib')
endif
