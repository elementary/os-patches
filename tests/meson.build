integration_test = find_program('integration-test')

envs = environment()
envs.set ('top_builddir', meson.build_root())
envs.set ('top_srcdir', meson.source_root())

test('iio-sensor-proxy-integration-test',
  integration_test,
  env: envs
  )
