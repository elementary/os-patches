installed = get_option('installed-tests')
executable('testcloudproviderserver', 'testcloudproviderserver.c',
  dependencies: libcloudproviders_dep,
  install: installed,
)
executable('testcloudproviderclient', 'testcloudproviderclient.c',
  dependencies: libcloudproviders_dep,
  install: installed,
)

service_conf = configuration_data ()
service_conf.set ('bindir', join_paths (prefix, bindir))
configure_file(
  input: 'org.freedesktop.CloudProviderServerExample.service.in',
  output: 'org.freedesktop.CloudProviderServerExample.service',
  configuration: service_conf,
  install_dir: servicedir,
  install: installed,
)

if installed
  cloudprovidersdata = join_paths (datadir, 'cloud-providers')
  install_data('org.freedesktop.CloudProviderServerExample.ini', install_dir : cloudprovidersdata)
endif
