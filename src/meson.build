deps = [ gio_dep, gudev_dep, mathlib_dep ]

resources = gnome.compile_resources(
    'iio-sensor-proxy-resources', 'iio-sensor-proxy.gresource.xml',
    c_name: 'iio_sensor_proxy',
    source_dir: '.',
    export: true
)

sources = [
  'iio-sensor-proxy.c',
  'drivers.c',
  'orientation.c',
  'drv-iio-buffer-accel.c',
  'drv-iio-poll-accel.c',
  'drv-input-accel.c',
  'drv-fake-compass.c',
  'drv-fake-light.c',
  'drv-iio-poll-light.c',
  'drv-hwmon-light.c',
  'drv-iio-buffer-light.c',
  'drv-iio-buffer-compass.c',
  'drv-iio-poll-proximity.c',
  'iio-buffer-utils.c',
  'accel-mount-matrix.c',
  'accel-scale.c',
  'accel-attributes.c',
  'utils.c',
  resources,
]

executable('iio-sensor-proxy',
  sources,
  dependencies: deps,
  install: true,
  install_dir: libexecdir
)

executable('fake-input-accelerometer',
  [ 'fake-input-accelerometer.c' ],
  dependencies: deps,
  install: false
)

executable('test-mount-matrix',
  [ 'test-mount-matrix.c', 'accel-mount-matrix.c' ],
  dependencies: deps,
  install: false
)

executable('test-orientation',
  [ 'test-orientation.c', 'orientation.c', 'accel-mount-matrix.c', 'accel-scale.c' ],
  dependencies: deps,
  install: false
)

if get_option('gtk-tests')
  executable('test-orientation-gtk',
    [ 'test-orientation-gtk.c', 'orientation.c', 'accel-scale.c' ],
    dependencies: [ deps, gtk_dep ],
    install: false
  )
endif

executable('monitor-sensor',
  [ 'monitor-sensor.c' ],
  dependencies: gio_dep,
  install: true,
  install_dir: bindir
)
