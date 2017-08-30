cdata = configuration_data()
cdata.set('VERSION', meson.get_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml',
               configuration: cdata)

gnome.gtkdoc('libcloudproviders',
             src_dir: inc,
             main_xml: meson.source_root (),
             content_files: [join_paths(meson.data_dir(), '/cloudprovidermanager-dbus.xml')],
             install : true)
