lib_inc = include_directories('.')

enum_headers = files('bluetooth-enums.h')

headers = enum_headers + files(
  'bluetooth-client.h',
  'bluetooth-chooser.h',
  'bluetooth-chooser-button.h',
  'bluetooth-chooser-combo.h',
  'bluetooth-filter-widget.h',
  'bluetooth-settings-widget.h',
  'bluetooth-utils.h'
)

install_headers(
  headers,
  subdir: meson.project_name()
)

sources = files(
  'bluetooth-agent.c',
  'bluetooth-chooser.c',
  'bluetooth-chooser-button.c',
  'bluetooth-chooser-combo.c',
  'bluetooth-client.c',
  'bluetooth-filter-widget.c',
  'bluetooth-pairing-dialog.c',
  'bluetooth-settings-obexpush.c',
  'bluetooth-settings-row.c',
  'bluetooth-settings-widget.c',
  'bluetooth-utils.c',
  'pin.c'
)

built_sources = []

resource_data = files(
  'bluetooth-pairing-dialog.ui',
  'bluetooth-settings.css',
  'bluetooth-settings-row.ui',
  'settings.ui'
)

built_sources += gnome.compile_resources(
  'bluetooth-settings-resources',
  'bluetooth.gresource.xml',
  source_dir: '.',
  c_name: 'bluetooth_settings',
  dependencies: resource_data,
  export: true
)

enum_types = 'gnome-bluetooth-enum-types'

built_sources += gnome.mkenums(
  enum_types + '.c',
  sources: enum_headers,
  fhead: '#include <bluetooth-enums.h>\n#include "gnome-bluetooth-enum-types.h"\n#include <glib-object.h>',
  fprod: '\n/* enumerations from "@filename@" */',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {',
  vprod: '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static ("@EnumName@", values);\n  }\n  return etype;\n}\n'
)

built_sources += gnome.mkenums(
  enum_types + '.h',
  sources: enum_headers,
  fhead: '#ifndef __GNOME_BLUETOOTH_ENUM_TYPES_H__\n#define __GNOME_BLUETOOTH_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void);\n#define BLUETOOTH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n\n#endif /* __GNOME_BLUETOOTH_ENUM_TYPES_H__ */'
)

client = 'bluetooth-client'

built_sources += gnome.gdbus_codegen(
  client + '-glue',
  client + '.xml',
  interface_prefix: 'org.bluez'
)

fdo = 'bluetooth-fdo'

built_sources += gnome.gdbus_codegen(
  fdo + '-glue',
  fdo + '.xml',
  interface_prefix: 'org.freedesktop.DBus'
)

deps = [
  gtk_dep,
  m_dep,
  dependency('gio-unix-2.0'),
  dependency('gmodule-2.0'),
  dependency('libcanberra-gtk3'),
  dependency('libnotify', version: '>= 0.7.0'),
  dependency('libudev')
]

cflags = [
  '-DPKGDATADIR="@0@"'.format(gnomebt_pkgdatadir),
  '-DG_LOG_DOMAIN="Bluetooth"'
]

symbol_map = meson.project_name() + '.map'

ldflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), symbol_map)
ldflags = []

if host_machine.system().contains('linux') and cc.has_argument(ldflag)
  ldflags += ldflag
endif

libgnome_bluetooth = shared_library(
  meson.project_name(),
  sources: sources + built_sources,
  version: libversion,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  link_args: ldflags,
  link_depends: symbol_map,
  install: true,
  install_dir: gnomebt_libdir
)

libgnome_bluetooth_dep = declare_dependency(
  link_with: libgnome_bluetooth,
  include_directories: lib_inc,
  dependencies: deps
)

pkg.generate(
  libraries: libgnome_bluetooth,
  version: gnomebt_version,
  name: gnomebt_api_name,
  description: 'Widgets for Bluetooth device selection',
  filebase: gnomebt_api_name,
  subdirs: meson.project_name(),
  requires: [
    'gio-2.0',
    'gtk+-3.0'
  ],
  requires_private: 'libudev',
  variables: 'exec_prefix=' + gnomebt_libexecdir,
  install_dir: join_paths(gnomebt_libdir, 'pkgconfig')
)

if enable_gir
  gir_sources = sources + headers

  gir_incs = [
    'GModule-2.0',
    'GObject-2.0',
    'Gtk-3.0'
  ]

  gir_extra_args = '--warn-all'

  gir_dir = join_paths(gnomebt_datadir, 'gir-' + gnomebt_gir_version)
  typelib_dir = join_paths(gnomebt_libdir, 'girepository-' + gnomebt_gir_version)

  gnome.generate_gir(
    libgnome_bluetooth,
    sources: gir_sources,
    nsversion: gnomebt_api_version,
    namespace: gnomebt_gir_ns,
    symbol_prefix: 'bluetooth',
    identifier_prefix: 'Bluetooth',
    export_packages: gnomebt_api_name,
    includes: gir_incs,
    extra_args: gir_extra_args,
    install: true,
    install_dir_gir: gir_dir,
    install_dir_typelib: typelib_dir,
  )
endif

test_names = [
  'test-agent',
  'test-class',
  'test-client',
  'test-deviceselection',
  'test-pairing-dialog',
  'test-pin',
  'test-settings'
]

foreach name: test_names
  executable(
    name,
    [name + '.c'] + built_sources,
    include_directories: top_inc,
    dependencies: deps,
    c_args: cflags,
    link_with: libgnome_bluetooth
  )
endforeach

database = 'pin-code-database.xml'

install_data(
  database,
  install_dir: gnomebt_pkgdatadir
)

custom_target(
  database,
  input: database,
  output: database + '.valid',
  command: [find_program('xmllint'), '--output', '@OUTPUT@', '--valid', '@INPUT']
)
