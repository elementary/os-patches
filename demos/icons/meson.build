libchamplain_demo_data = [
  'emblem-favorite.png',
  'emblem-generic.png',
  'emblem-important.png',
  'emblem-new.png',
]

custom_target(
  'libchamplain-demo-data',
  input: libchamplain_demo_data,
  output: libchamplain_demo_data,
  command: ['cp', '@INPUT@', '@OUTDIR@'],
  build_by_default: true,
)