i18n.merge_file('com.endlessm.ParentalControls.policy',
  input: 'com.endlessm.ParentalControls.policy.in',
  output: 'com.endlessm.ParentalControls.policy',
  po_dir: po_dir,
  install: true,
  install_dir: polkitpolicydir,
)

dbus_interfaces = [
  'com.endlessm.ParentalControls.AccountInfo',
  'com.endlessm.ParentalControls.AppFilter',
  'com.endlessm.ParentalControls.SessionLimits',
]

foreach dbus_interface: dbus_interfaces
  filename = dbus_interface + '.xml'
  install_data(filename,
    install_dir: dbusinterfacesdir)
  meson.add_install_script(meson_make_symlink,
    join_paths(dbusinterfacesdir, filename),
    join_paths(accountsserviceinterfacesdir, filename))
endforeach

polkit_conf = configuration_data()
polkit_conf.set('PRIVILEGED_GROUP', get_option('privileged_group'))
configure_file(
  input: 'com.endlessm.ParentalControls.rules.in',
  output: 'com.endlessm.ParentalControls.rules',
  configuration: polkit_conf,
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'rules.d')
)
