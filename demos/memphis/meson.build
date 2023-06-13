libchamplain_memphis_data = [
  'default-rules.xml',
  'high-contrast.xml',
  'las_palmas.osm',
  'schaffhausen.osm',
]

custom_target(
  'libchamplain_memphis_data',
  input: libchamplain_memphis_data,
  output: libchamplain_memphis_data,
  command: ['cp', '@INPUT@', '@OUTDIR@'],
  build_by_default: true,
)
