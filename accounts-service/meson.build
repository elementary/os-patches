i18n.merge_file('com.endlessm.ParentalControls.policy',
  input: 'com.endlessm.ParentalControls.policy.in',
  output: 'com.endlessm.ParentalControls.policy',
  po_dir: po_dir,
  install: true,
  install_dir: polkitpolicydir,
)

install_data('com.endlessm.ParentalControls.AppFilter.xml',
  install_dir: dbusinterfacesdir)
meson.add_install_script(meson_make_symlink,
  join_paths(dbusinterfacesdir, 'com.endlessm.ParentalControls.AppFilter.xml'),
  join_paths(accountsserviceinterfacesdir, 'com.endlessm.ParentalControls.AppFilter.xml'))

install_data('com.endlessm.ParentalControls.rules',
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'rules.d'))
