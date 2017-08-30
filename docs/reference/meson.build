cdata = configuration_data()
cdata.set('VERSION', meson.project_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml',
               configuration: cdata)

codegen = find_program('gdbus-codegen')
message('Generating DBUS docs…')
run_command('gdbus-codegen', '--generate-docbook', 'generated-docs',
            join_paths(meson.source_root(), 'data/cloudprovider-dbus.xml'),
            join_paths(meson.source_root(), 'data/cloudprovidermanager-dbus.xml'),
            '--output-directory', join_paths(meson.source_root(), 'docs/reference'))

gnome.gtkdoc('libcloudproviders',
             src_dir: join_paths (meson.source_root(), 'src'),
             main_xml: 'libcloudproviders-docs.xml',
             content_files: ['generated-docs-org.freedesktop.CloudProvider.Account1.xml',
                             'generated-docs-org.freedesktop.CloudProvider.Manager1.xml'],
             install : true)
