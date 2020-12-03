deps = [
  dependency('gio-2.0', version: '>= 2.60.1'),
  dependency('gio-unix-2.0', version: '>= 2.44'),
  dependency('glib-2.0', version: '>= 2.60.0'),
  dependency('gobject-2.0', version: '>= 2.44'),
  dependency('glib-testing-0', fallback: ['libglib-testing', 'libglib_testing_dep']),
  libmalcontent_dep,
]

envs = test_env + [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
]

gdbus_codegen = find_program('gdbus-codegen')

accounts_service_iface_h = custom_target(
  'accounts-service-iface.h',
  input: ['org.freedesktop.Accounts.xml'],
  output: ['accounts-service-iface.h'],
  command: [gdbus_codegen,
            '--interface-info-header',
            '--output', '@OUTPUT@',
            '@INPUT@'],
)
accounts_service_iface_c = custom_target(
  'accounts-service-iface.c',
  input: ['org.freedesktop.Accounts.xml'],
  output: ['accounts-service-iface.c'],
  command: [gdbus_codegen,
            '--interface-info-body',
            '--output', '@OUTPUT@',
            '@INPUT@'],
)

accounts_service_extension_ifaces = [
  join_paths(meson.current_source_dir(), '..', '..', 'accounts-service', 'com.endlessm.ParentalControls.AppFilter.xml'),
  join_paths(meson.current_source_dir(), '..', '..', 'accounts-service', 'com.endlessm.ParentalControls.SessionLimits.xml'),
]

accounts_service_extension_iface_h = custom_target(
  'accounts-service-extension-iface.h',
  input: accounts_service_extension_ifaces,
  output: ['accounts-service-extension-iface.h'],
  command: [gdbus_codegen,
            '--interface-info-header',
            '--output', '@OUTPUT@',
            '@INPUT@'],
)
accounts_service_extension_iface_c = custom_target(
  'accounts-service-extension-iface.c',
  input: accounts_service_extension_ifaces,
  output: ['accounts-service-extension-iface.c'],
  command: [gdbus_codegen,
            '--interface-info-body',
            '--output', '@OUTPUT@',
            '@INPUT@'],
)

test_programs = [
  ['app-filter', [
    accounts_service_iface_h,
    accounts_service_iface_c,
    accounts_service_extension_iface_h,
    accounts_service_extension_iface_c,
  ], deps],
  ['session-limits', [
    accounts_service_iface_h,
    accounts_service_iface_c,
    accounts_service_extension_iface_h,
    accounts_service_extension_iface_c,
  ], deps],
]

installed_tests_metadir = join_paths(datadir, 'installed-tests',
                                     'libmalcontent-' + libmalcontent_api_version)
installed_tests_execdir = join_paths(libexecdir, 'installed-tests',
                                     'libmalcontent-' + libmalcontent_api_version)

foreach program: test_programs
  test_conf = configuration_data()
  test_conf.set('installed_tests_dir', installed_tests_execdir)
  test_conf.set('program', program[0])

  configure_file(
    input: test_template,
    output: program[0] + '.test',
    install: enable_installed_tests,
    install_dir: installed_tests_metadir,
    configuration: test_conf,
  )

  exe = executable(
    program[0],
    [program[0] + '.c'] + program[1],
    dependencies: program[2],
    include_directories: root_inc,
    install: enable_installed_tests,
    install_dir: installed_tests_execdir,
  )

  test(
    program[0],
    exe,
    env: envs,
    args: ['--tap'],
  )
endforeach