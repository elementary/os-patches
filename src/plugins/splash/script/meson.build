script_inputs = [
  'image',
  'math',
  'plymouth',
  'sprite',
  'string',
]

generate_script_string_header_py = find_program('generate_script_string_header.py')

script_headers = []
foreach s : script_inputs
  s_input_file = 'script-lib-@0@.script'.format(s)

  s_header = custom_target(
    input: s_input_file,
    output: s_input_file + '.h',
    command: [
      generate_script_string_header_py,
      '@INPUT@',
    ],
    capture: true,
  )

  script_headers += s_header
endforeach

script_plugin_src = files(
  'plugin.c',
  'script-debug.c',
  'script-execute.c',
  'script-lib-image.c',
  'script-lib-math.c',
  'script-lib-plymouth.c',
  'script-lib-sprite.c',
  'script-lib-string.c',
  'script-object.c',
  'script-parse.c',
  'script-scan.c',
  'script.c',
)

script_plugin = shared_module('script',
  [ script_headers, script_plugin_src ],
  dependencies: [
    libply_splash_core_dep,
    libply_splash_graphics_dep,
  ],
  c_args: [
    '-DPLYMOUTH_LOGO_FILE="@0@"'.format(plymouth_logo_file),
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: true,
  install_dir: plymouth_plugin_path,
)
