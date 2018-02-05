src_inc = include_directories('.')

# FIXME: although this file is contained in the libnma directory
#        it should be generated before because it's used at
#        wireless-security directory, which is needed later for
#        libnma directory.
libnma_inc = include_directories('libnma')

version_conf = configuration_data()
version_conf.set('NMA_MAJOR_VERSION', nma_major_version)
version_conf.set('NMA_MINOR_VERSION', nma_minor_version)
version_conf.set('NMA_MICRO_VERSION', nma_micro_version)

version = 'nma-version.h'

version_header = configure_file(
  input: 'libnma/' + version + '.in',
  output: version,
  configuration: version_conf,
  install: true,
  install_dir: join_paths(nma_includedir, 'libnma')
)

subdir('utils')
subdir('wireless-security')
subdir('libnma')

if enable_libnm_gtk
  subdir('libnm-gtk')
endif

subdir('connection-editor')

sources = files(
  'ap-menu-item.c',
  'applet-agent.c',
  'applet.c',
  'applet-device-bt.c',
  'applet-device-ethernet.c',
  'applet-device-wifi.c',
  'applet-dialogs.c',
  'applet-vpn-request.c',
  'ethernet-dialog.c',
  'main.c',
  'mb-menu-item.c',
  'mobile-helpers.c'
)

resource_data = files(
  '8021x.ui',
  'connection-editor/ce-page-dsl.ui',
  'gsm-unlock.ui',
  'info.ui'
)

sources += gnome.compile_resources(
  'applet-resources',
  'applet.gresource.xml',
  dependencies: resource_data
)

incs = [
  top_inc,
  utils_inc,
  src_inc,
  shared_inc,
  wireless_security_inc
]

deps = [
  gtk_dep,
  libnm_dep,
  libnma_dep,
  libnotify_dep,
  libsecret_dep,
  m_dep
]

cflags = [
  '-DBINDIR="@0@"'.format(nma_bindir),
  '-DDATADIR="@0@"'.format(nma_datadir),
  '-DLIBDIR="@0@"'.format(nma_libdir),
  '-DLIBEXECDIR="@0@"'.format(nma_libexecdir),
  '-DNMALOCALEDIR="@0@"'.format(nma_localedir),
  '-DSYSCONFDIR="@0@"'.format(nma_sysconfdir),
  '-DAUTOSTARTDIR="@0@"'.format(nma_autostartdir),
  '-DICONDIR="@0@"'.format(nma_icondir),
  '-DG_LOG_DOMAIN="@0@"'.format(nma_name)
]

ldflags = []
if have_version_script
  ldflags += '-Wl,--version-script,@0@'.format(linker_script_ver)
endif

if enable_appindicator
  deps += [
    appindicator_dep,
    dbusmenu_dep
  ]
endif

if enable_wwan
  sources += files('applet-device-broadband.c')

  deps += mm_glib_dep
endif

executable(
  nma_name,
  sources,
  include_directories: incs,
  dependencies: deps,
  c_args: cflags,
  link_args: ldflags,
  link_depends: linker_script_ver,
  link_whole: libwireless_security_libnm,
  install: true,
  install_dir: nma_bindir
)
