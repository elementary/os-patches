libply_sources = files(
  'ply-array.c',
  'ply-bitarray.c',
  'ply-buffer.c',
  'ply-command-parser.c',
  'ply-event-loop.c',
  'ply-hashtable.c',
  'ply-key-file.c',
  'ply-list.c',
  'ply-logger.c',
  'ply-progress.c',
  'ply-rectangle.c',
  'ply-region.c',
  'ply-terminal-session.c',
  'ply-trigger.c',
  'ply-utils.c',
)

libply_deps = [
  ldl_dep,
  lm_dep,
]

libply = library('ply',
  libply_sources,
  dependencies: libply_deps,
  include_directories: config_h_inc,
  version: plymouth_soversion,
  install: true,
)

libply_dep = declare_dependency(
  link_with: libply,
  dependencies: libply_deps,
  include_directories: include_directories('.'),
)

libply_headers = files(
  'ply-array.h',
  'ply-bitarray.h',
  'ply-buffer.h',
  'ply-command-parser.h',
  'ply-event-loop.h',
  'ply-hashtable.h',
  'ply-i18n.h',
  'ply-key-file.h',
  'ply-list.h',
  'ply-logger.h',
  'ply-progress.h',
  'ply-rectangle.h',
  'ply-region.h',
  'ply-terminal-session.h',
  'ply-trigger.h',
  'ply-utils.h',
)

install_headers(libply_headers,
  install_dir: get_option('includedir') / 'plymouth-1' / 'ply',
)
