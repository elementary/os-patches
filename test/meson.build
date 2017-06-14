gtk = dependency('gtk+-3.0')
gtkcloudprovidertest_deps = [glib, gio, gio_unix, gtk]
dep = declare_dependency (link_with: libgtkcloudproviders,
                          include_directories: include_directories('../src/'),
                          dependencies: gtkcloudprovidertest_deps)

executable('testcloudproviderserver', 'testcloudproviderserver.c', dependencies: dep, install: true)
executable('testcloudproviderclient', 'testcloudproviderclient.c', dependencies: dep, install: true)

service_conf = configuration_data ()
service_conf.set ('bindir', join_paths (prefix, bindir))
configure_file(input: 'org.gtk.CloudProviderServerExample.service.in',
                output: 'org.gtk.CloudProviderServerExample.service',
                configuration: service_conf,
                install_dir: servicedir)

cloudprovidersdata = join_paths (datadir, 'gtk+', 'cloud-providers')
install_data('org.gtk.CloudProviderServerExample.ini', install_dir : cloudprovidersdata)
