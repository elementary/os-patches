deps = [
  dependency('glib-2.0', version: '>= 2.60.0'),
  cc.find_library('dl'),
]

envs = test_env + [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
]

test_programs = [
  ['pam_malcontent', [], deps],
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
