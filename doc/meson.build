cdata = configuration_data()
cdata.set('VERSION', '1.0')
configure_file(input : 'version.xml.in',
  output : 'version.xml',
  configuration : cdata)

inc = include_directories('../src')

gnome.gtkdoc('libcloudproviders', src_dir : inc,
main_sgml : 'libcloudproviders-docs.sgml',
content_files: ['../data/cloudprovidermanager-dbus.xml'],
install : true)
