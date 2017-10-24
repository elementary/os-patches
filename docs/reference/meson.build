cdata = configuration_data()
cdata.set('VERSION', meson.project_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml',
               configuration: cdata)

codegen = find_program('gdbus-codegen')
message ('----------------------------')
message('Generating DBUS docs…')
run_command('gdbus-codegen', '--generate-docbook', 'generated-docs',
            join_paths(meson.source_root(), 'data/cloud-providers-dbus.xml'),
            '--output-directory', join_paths(meson.source_root(), 'docs/reference'))

gnome.gtkdoc('libcloudproviders',
             src_dir: join_paths (meson.source_root(), 'src'),
             dependencies: [libcloudproviders_dep],
             gobject_typesfile: 'libcloudproviders.types',
             main_xml: 'libcloudproviders-docs.xml',
             content_files: ['generated-docs-org.freedesktop.CloudProviders.Account.xml',
                             'generated-docs-org.freedesktop.CloudProviders.Provider.xml'],
             install : true,
             scan_args: ['--rebuild-types'])

message ('----------------------------')
