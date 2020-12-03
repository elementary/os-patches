libpam = cc.find_library('pam', required: true)
libpam_misc = cc.find_library('pam_misc', required: true)

pam_malcontent = shared_library('pam_malcontent',
  files('pam_malcontent.c'),
  name_prefix: '',
  link_args: [
    '-shared',
    '-Wl,--version-script=' + join_paths(meson.current_source_dir(), 'pam_malcontent.sym'),
  ],
  dependencies: [
    dependency('gio-2.0', version: '>= 2.44'),
    dependency('glib-2.0', version: '>= 2.54.2'),
    dependency('gobject-2.0', version: '>= 2.54'),
    libmalcontent_dep,
    libpam,
    libpam_misc,
  ],
  link_depends: files('pam_malcontent.sym'),
  include_directories: root_inc,
  install: true,
  install_dir: pamlibdir)

subdir('tests')
