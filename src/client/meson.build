plymouth_client_src = files(
  'ply-boot-client.c',
  'plymouth.c',
)

plymouth_client = executable('plymouth',
  plymouth_client_src,
  dependencies: libply_dep,
  include_directories: [
    config_h_inc,
    include_directories('..'),
  ],
  install: true,
)


libply_boot_client_src = files(
  'ply-boot-client.c',
)

libply_boot_client = library('ply-boot-client',
  libply_boot_client_src,
  dependencies: [ libply_dep ],
  include_directories: [
    config_h_inc,
    include_directories('..'),
  ],
  version: plymouth_soversion,
  install: true,
)

libply_boot_client_dep = declare_dependency(
  link_with: libply_boot_client,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
)

libply_boot_client_headers = files(
  'ply-boot-client.h',
  '../ply-boot-protocol.h',
)

install_headers(libply_boot_client_headers,
  install_dir: get_option('includedir') / 'plymouth-1' / 'ply-boot-client',
)

pkgconfig.generate(libply_boot_client,
  filebase: 'ply-boot-client',
  name: 'Plymouth',
  description: 'Client Library for Boot Splash',
  libraries: [
    libply,
    ldl_dep,
  ],
  subdirs: [
    'plymouth-1/ply',
    'plymouth-1/ply-boot-client',
  ],
)
