application_id = 'org.freedesktop.MalcontentControl'

if not cc.has_function('atexit')
  error('atexit() needed for generated GResource files')
endif

resources = gnome.compile_resources(
  'resources',
  'malcontent-control.gresource.xml',
  source_dir: meson.current_source_dir(),
)

malcontent_control = executable('malcontent-control',
  [
    'application.c',
    'application.h',
    'carousel.c',
    'carousel.h',
    'main.c',
    'user-image.c',
    'user-image.h',
    'user-selector.c',
    'user-selector.h',
  ] + resources,
  dependencies: [
    dependency('accountsservice'),
    dependency('gio-2.0', version: '>= 2.44'),
    dependency('glib-2.0', version: '>= 2.54.2'),
    dependency('gobject-2.0', version: '>= 2.54'),
    dependency('gtk+-3.0'),
    dependency('polkit-gobject-1'),
    libmalcontent_dep,
    libmalcontent_ui_dep,
  ],
  include_directories: root_inc,
  install: true,
)

desktop_file = i18n.merge_file('desktop-file',
  type: 'desktop',
  input: '@0@.desktop.in'.format(application_id),
  output: '@0@.desktop'.format(application_id),
  po_dir: join_paths(meson.current_source_dir(), '..', 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
    'validate-desktop',
    desktop_file_validate,
    args: [
      desktop_file.full_path(),
    ],
    suite: ['malcontent-control'],
  )
endif

appdata_file = i18n.merge_file('appdata-file',
  input: '@0@.appdata.xml.in'.format(application_id),
  output: '@0@.appdata.xml'.format(application_id),
  po_dir: join_paths(meson.current_source_dir(), '..', 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata', appstream_util,
    args: [
      'validate-relax', '--nonet', appdata_file.full_path(),
    ],
    suite: ['malcontent-control'],
  )
endif

xmllint = find_program('xmllint', required: false)
if xmllint.found()
  gtk_prefix = dependency('gtk+-3.0').get_pkgconfig_variable('prefix')
  test(
    'validate-ui', xmllint,
    args: [
      '--nonet', '--noblanks', '--noout',
      '--relaxng', join_paths(gtk_prefix, 'share', 'gtk-3.0', 'gtkbuilder.rng'),
      files(
        'carousel.ui',
        'main.ui',
        'user-selector.ui',
      ),
    ],
    suite: ['malcontent-control'],
  )
endif

policy_file = i18n.merge_file('policy-file',
  input: '@0@.policy.in'.format(application_id),
  output: '@0@.policy'.format(application_id),
  po_dir: join_paths(meson.current_source_dir(), '..', 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'actions'),
)
if xmllint.found()
  test(
    'validate-policy', xmllint,
    args: [
      '--nonet', '--noblanks', '--noout',
      policy_file,
    ],
    suite: ['malcontent-control'],
  )
endif

# FIXME: Add tests
subdir('icons')
#subdir('tests')
