project(
  'network-manager-applet', 'c',
  version: '1.8.10',
  license: 'GPL2+',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu99'
  ],
  meson_version: '>= 0.43.0'
)

nma_name = 'nm-applet'

nma_version = meson.project_version()
version_array = nma_version.split('.')
nma_major_version = version_array[0].to_int()
nma_minor_version = version_array[1].to_int()
nma_micro_version = version_array[2].to_int()

nma_gir_version = '1.0'
nma_gir_prefix = 'NMA'

nma_gir_ns = 'NMA'
nma_gtk_gir_ns = 'NMGtk'

nma_prefix = get_option('prefix')
nma_bindir = join_paths(nma_prefix, get_option('bindir'))
nma_datadir = join_paths(nma_prefix, get_option('datadir'))
nma_includedir = join_paths(nma_prefix, get_option('includedir'))
nma_libdir = join_paths(nma_prefix, get_option('libdir'))
nma_libexecdir = join_paths(nma_prefix, get_option('libexecdir'))
nma_localedir = join_paths(nma_prefix, get_option('localedir'))
nma_mandir = join_paths(nma_prefix, get_option('mandir'))
nma_sysconfdir = join_paths(nma_prefix, get_option('sysconfdir'))

nma_appdir = join_paths(nma_datadir, 'applications')
nma_autostartdir = join_paths(nma_sysconfdir, 'xdg', 'autostart')
nma_icondir = join_paths(nma_datadir, 'icons')

soversion = 0
current = 0
revision = 0
libversion = '@0@.@1@.@2@'.format(soversion, current, revision)

nma_debug = get_option('buildtype').contains('debug')

cc = meson.get_compiler('c')

config_h = configuration_data()

# defines
config_h.set_quoted('GETTEXT_PACKAGE', nma_name)
config_h.set_quoted('VERSION', nma_version)

# types
if not cc.has_type('pid_t', prefix: '#include<sys/types.h>')
  config_h.set('pid_t', 'int')
endif

# compiler flags
common_flags = []
common_ldflags = []

enable_ld_gc = get_option('ld_gc')
if enable_ld_gc
  test_cflags = [
    '-fdata-sections',
    '-ffunction-sections',
  ]

  test_ldflags = ['-Wl,--gc-sections']

  foreach cflag: test_cflags + test_ldflags
    assert(cc.has_argument(cflag), 'Unused symbol eviction requested but not supported. Use -Dld_gc=false to build without it.')
  endforeach

  common_flags += test_cflags
  common_ldflags += test_ldflags
endif

if nma_debug
  test_cflags = [
    '-Wdeclaration-after-statement',
    '-Wfloat-equal',
    '-Wformat=2',
    '-Wimplicit-fallthrough',
    '-Winit-self',
    '-Wmissing-declarations',
    '-Wmissing-include-dirs',
    '-Wmissing-prototypes',
    '-Wno-duplicate-decl-specifier',
    '-Wno-format-truncation',
    '-Wno-format-y2k',
    '-Wno-missing-field-initializers',
    '-Wno-pragmas',
    '-Wno-sign-compare',
    '-Wno-unused-but-set-variable',
    '-Wno-unused-parameter',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wundef'
  ]

  common_flags += cc.get_supported_arguments(test_cflags)
endif

add_project_arguments(common_flags, language: 'c')
add_project_link_arguments(common_ldflags, language: 'c')

ldflag = '-Wl,--version-script'
have_version_script = host_machine.system().contains('linux') and cc.has_argument(ldflag)

linker_script_ver = join_paths(meson.source_root(), 'linker-script-binary.ver')

# Check for iso-codes for country names translation
enable_iso_codes = get_option('iso_codes')
message('whether to disable iso-codes at build-time: ' + enable_iso_codes.to_string())
if enable_iso_codes
  iso_codes_dep = dependency('iso-codes')

  str = 'Consider installing the package or adjusting the PKG_CONFIG_PATH environment variable.\n'
  str += 'You can also disable build-time check for \'iso-codes\' via -Diso_codes=false'
  assert(iso_codes_dep.found(), str)

  iso_3166 = iso_codes_dep.get_pkgconfig_variable('domains').contains('iso_3166')
  message('whether iso-codes has iso_3166 domain: ' + iso_3166.to_string())

  config_h.set_quoted('ISO_CODES_PREFIX', iso_codes_dep.get_pkgconfig_variable('prefix'))
else
  config_h.set_quoted('ISO_CODES_PREFIX', nma_prefix)
endif

gio_dep = dependency('gio-2.0', version: '>= 2.38')
gmodule_export_dep = dependency('gmodule-export-2.0')
libsecret_dep = dependency('libsecret-1', version: '>= 0.18')

m_dep = cc.find_library('m')

# Check for gtk+
gtk_req_version = '>= 3.4'

gtk_dep = declare_dependency(
  dependencies: dependency('gtk+-3.0', version: gtk_req_version),
  compile_args: [
    '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_4',
    '-DGTK_VERSION_MAX_ALLOWED=GTK_VERSION_3_4'
  ]
)

# Check for gudev
gudev_dep = dependency('gudev-1.0', version: '>= 147')

# Check for libnotify >= 0.7
libnotify_dep = dependency('libnotify', version: '>= 0.4.3')
config_h.set10('HAVE_LIBNOTIFY_07', libnotify_dep.found() and libnotify_dep.version().version_compare('>= 0.7'),
               description: 'Define if you have libnotify 0.7 or later')

# API documentation
nm_req_version = '>= 1.7'

deps = [
  gio_dep,
  gmodule_export_dep,
  dependency('libnm', version: nm_req_version)
]

cflags = [
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_38',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_38',
  '-DNM_VERSION_MIN_REQUIRED=NM_VERSION_1_8',
  '-DNM_VERSION_MAX_ALLOWED=NM_VERSION_1_8'
]

libnm_dep = declare_dependency(
  dependencies: deps,
  compile_args: cflags
)

enable_libnm_gtk = get_option('libnm_gtk')
if enable_libnm_gtk
  deps = [
    gio_dep,
    gmodule_export_dep,
    dependency('libnm-glib', version: nm_req_version),
    dependency('libnm-glib-vpn', version: nm_req_version),
    dependency('libnm-util', version: nm_req_version),
    dependency('NetworkManager', version: nm_req_version)
  ]

  cflags = [
    '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_38',
    '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_38',
    '-DNM_VERSION_MIN_REQUIRED=NM_VERSION_1_4',
    '-DNM_VERSION_MAX_ALLOWED=NM_VERSION_1_4'
  ]

  libnm_glib_dep = declare_dependency(
    dependencies: deps,
    compile_args: cflags
  )
endif

enable_appindicator = get_option('appindicator')
if enable_appindicator
  appindicator_dep = dependency('appindicator3-0.1')
  dbusmenu_dep = dependency('dbusmenu-gtk3-0.4', version: '>= 16.04.0')
endif
config_h.set('WITH_APPINDICATOR', enable_appindicator)

# ModemManager1 with libmm-glib for WWAN support
enable_wwan = get_option('wwan')
if enable_wwan
  mm_glib_dep = dependency('mm-glib', required: false)
  assert(mm_glib_dep.found(), 'libmm-glib is needed for WWAN support. Use -Dwwan=false to build without it.')
endif
config_h.set10('WITH_WWAN', enable_wwan)

# SELinux
enable_selinux = get_option('selinux')
if enable_selinux
  libselinux_dep = dependency('libselinux', required: false)
  assert(libselinux_dep.found(), 'libselinux is needed for SELinux label support in configuration editor. Use -Dselinux=false to build without it.')
endif
config_h.set10('WITH_SELINUX', enable_selinux)

# Jansson for team configuration editing
enable_team = get_option('team')
if enable_team
  jansson_dep = dependency('jansson', version: '>= 2.7', required: false)
  assert(jansson_dep.found(), 'jansson is needed for team configuration editor. Use -Dteam=false to build without it.')
endif
config_h.set10('WITH_JANSSON', enable_team)

# GCR for PKCS#11 enabled certificate chooser
enable_gcr = get_option('gcr')
if enable_gcr
  deps = [
    dependency('gcr-3', version: '>= 3.14'),
    dependency('gck-1', version: '>= 3.14')
  ]

  cflags = [
    '-DGCR_API_SUBJECT_TO_CHANGE',
    '-DGCK_API_SUBJECT_TO_CHANGE'
  ]

  # GCR API is declared subject to change, do an extensive check of the prototypes
  gcr_src = '''
    #ifndef GCR_API_SUBJECT_TO_CHANGE
    # define GCR_API_SUBJECT_TO_CHANGE
    #endif
    #ifndef GCK_API_SUBJECT_TO_CHANGE
    # define GCK_API_SUBJECT_TO_CHANGE
    #endif

    #include <gcr/gcr.h>
    #include <gck/gck.h>

    const GckAttribute *gck_attributes_find (GckAttributes *attrs, gulong attr_type);
    gboolean gck_attributes_find_string (GckAttributes *attrs, gulong attr_type, gchar **value);
    gboolean gck_attributes_find_ulong (GckAttributes *attrs, gulong attr_type, gulong *value);
    GckAttributes *gck_attributes_new_empty (gulong first_type, ...);
    void gck_attributes_unref (gpointer attrs);
    void gck_builder_add_all (GckBuilder *builder, GckAttributes *attrs);
    void gck_builder_add_only (GckBuilder *builder, GckAttributes *attrs, gulong only_type, ...);
    GckAttributes *gck_builder_end (GckBuilder *builder);
    GckBuilder *gck_builder_new (GckBuilderFlags flags);
    void gck_enumerator_next_async (GckEnumerator *self, gint max_objects, GCancellable *cancellable,
                                    GAsyncReadyCallback callback, gpointer user_data);
    GList *gck_enumerator_next_finish (GckEnumerator *self, GAsyncResult *result, GError **error);
    void gck_list_unref_free (GList *reflist);
    GList *gck_modules_get_slots (GList *modules, gboolean token_present);
    void gck_modules_initialize_registered_async (GCancellable *cancellable, GAsyncReadyCallback callback,
                                                  gpointer user_data);
    GList *gck_modules_initialize_registered_finish (GAsyncResult *result, GError **error);
    void gck_object_get_async (GckObject *self, const gulong *attr_types, guint n_attr_types,
                               GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
    GckAttributes *gck_object_get_finish (GckObject *self, GAsyncResult *result, GError **error);
    void gck_session_login_async (GckSession *self, gulong user_type, const guchar *pin,
                                  gsize n_pin, GCancellable *cancellable,
                                  GAsyncReadyCallback callback, gpointer user_data);
    gboolean gck_session_login_finish (GckSession *self, GAsyncResult *result, GError **error);
    GckSlotInfo *gck_slot_get_info (GckSlot *self);
    GckTokenInfo *gck_slot_get_token_info (GckSlot *self);
    void gck_slot_open_session_async (GckSlot *self, GckSessionOptions options, GCancellable *cancellable,
                                      GAsyncReadyCallback callback, gpointer user_data);
    GckSession *gck_slot_open_session_finish (GckSlot *self, GAsyncResult *result, GError **error);
    void gck_token_info_free (GckTokenInfo *token_info);
    gchar *gck_uri_build (GckUriData *uri_data, GckUriFlags flags);
    void gck_uri_data_free (GckUriData *uri_data);
    GckUriData *gck_uri_parse (const gchar *string, GckUriFlags flags, GError **error);
    gchar *gcr_certificate_get_issuer_name (GcrCertificate *self);
    gchar *gcr_certificate_get_subject_name (GcrCertificate *self);
    GcrCertificate *gcr_simple_certificate_new (const guchar *data, gsize n_data);
  '''

  assert(cc.compiles(gcr_src, dependencies: deps), 'gcr support was requested, but the gcr library is not available. Use -Dgcr=false to build without it.')

  gcr_dep = declare_dependency(
    dependencies: deps,
    compile_args: cflags
  )
endif
config_h.set10('WITH_GCR', enable_gcr)

# introspection support
enable_introspection = get_option('introspection')
if enable_introspection
  gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.9.6', required: false)
  assert(gir_dep.found(), 'introspection support was requested, but the gobject-introspection library is not available. Use -Dintrospection=false to build without it.')
endif

more_asserts = get_option('more_asserts')
if more_asserts == 'no'
  more_asserts = 0
elif more_asserts == 'yes'
  more_asserts = 100
else
  more_asserts = more_asserts.to_int()
endif
config_h.set('NM_MORE_ASSERTS', more_asserts)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

po_dir = join_paths(meson.source_root(), 'po')

intltool_merge = find_program('intltool-merge')
intltool_cache = join_paths(po_dir, '.intltool-merge-cache')
intltool_desktop_cmd = [intltool_merge, '-d', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@']
intltool_xml_cmd = [intltool_merge, '-x', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@']

top_inc = include_directories('.')

subdir('po')
subdir('icons')
subdir('shared')
subdir('src')
subdir('man')

desktop = 'nm-applet.desktop'

# FIXME: The same target can not be copied into two directories.
#        There is a workaround in meson_post_install.py until proper solution arises:
#        https://groups.google.com/forum/#!topic/mesonbuild/3iIoYPrN4P0
custom_target(
  desktop,
  input: desktop + '.in',
  output: desktop,
  command: intltool_desktop_cmd,
  install: true,
  install_dir: nma_appdir
  #install_dir: [
  #  nma_appdir,
  #  nma_autostartdir
  #]
)

desktop = 'nm-connection-editor.desktop'

custom_target(
  desktop,
  input: desktop + '.in',
  output: desktop,
  command: intltool_desktop_cmd,
  install: true,
  install_dir: nma_appdir
)

appdata = 'nm-connection-editor.appdata.xml'

custom_target(
  appdata,
  input: appdata + '.in',
  output: appdata,
  command: intltool_xml_cmd,
  install: true,
  install_dir: join_paths(nma_datadir, 'appdata')
)

schema_conf = configuration_data()
schema_conf.set('GETTEXT_PACKAGE', nma_name)

schema = 'org.gnome.nm-applet.gschema.xml'

configure_file(
  input: schema + '.in',
  output: schema,
  install: true,
  install_dir: join_paths(nma_datadir, 'glib-2.0', 'schemas'),
  configuration: schema_conf
)

install_data(
  'nm-applet.convert',
  install_dir: join_paths(nma_datadir, 'GConf', 'gsettings')
)

# documentation
enable_gtk_doc = get_option('gtk_doc')
if enable_gtk_doc
  mod_name = 'libnma'

  private_headers = [
    'nma-cert-chooser-button.h',
    'nma-cert-chooser-private.h',
    'nma-pkcs11-cert-chooser-dialog.h',
    'nma-pkcs11-token-login-dialog.h',
    'nma-resources.h',
    'nma-version.h'
  ]

  private_sources = [
    'nma-cert-chooser-button.c',
    'nma-file-cert-chooser.c',
    'nma-pkcs11-cert-chooser-dialog.c',
    'nma-pkcs11-cert-chooser.c',
    'nma-pkcs11-token-login-dialog.c',
    'nma-resources.c'
  ]

  ent_conf = configuration_data()
  ent_conf.set_quoted('PACKAGE', meson.project_name())
  ent_conf.set_quoted('PACKAGE_BUGREPORT', 'http://bugzilla.gnome.org/enter_bug.cgi?product=NetworkManager')
  ent_conf.set_quoted('PACKAGE_NAME', nma_name)
  ent_conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(nma_name, nma_version))
  ent_conf.set_quoted('PACKAGE_TARNAME', meson.project_name())
  ent_conf.set_quoted('PACKAGE_URL', '')
  ent_conf.set_quoted('PACKAGE_VERSION', nma_version)

  ent = 'gtkdocentities.ent'

  configure_file(
    input: ent + '.in',
    output: ent,
    configuration: ent_conf
  )

  doc_path = join_paths(nma_datadir, 'gtk-doc', 'html', mod_name)

  gnome.gtkdoc(
    mod_name,
    main_xml: mod_name + '-docs.xml',
    src_dir: libnma_inc,
    dependencies: libnma_dep,
    scan_args: [
      '--rebuild-types',
      '--rebuild-sections',
      '--ignore-headers=' + ' '.join(private_headers)
    ],
    mkdb_args: '--ignore-files=' + ' '.join(private_headers + private_sources),
    gobject_typesfile: mod_name + '.types',
    install: true,
    install_dir: doc_path
  )
endif

config = 'config.h'

configure_file(
  input: config + '.meson',
  output: config,
  configuration: config_h
)

meson.add_install_script(
  'meson_post_install.py',
  nma_datadir,
  nma_sysconfdir
)

output = '\n  Build legacy library libnm-gtk: ' + enable_libnm_gtk.to_string() + '\n'
output += '  GCR: ' + enable_gcr.to_string() + '\n'
output += '  LTO: ' + get_option('b_lto').to_string() + '\n'
output += '  Linker garbage collection: ' + enable_ld_gc.to_string()
message(output)
