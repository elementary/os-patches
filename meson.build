project('geoclue', 'c', version: '2.5.6', meson_version : '>= 0.47.2')

gclue_version = meson.project_version()
ver_arr = gclue_version.split('.')

gclue_major_version = ver_arr[0]
gclue_minor_version = ver_arr[1]
gclue_micro_version = ver_arr[2]

gclue_api_version='2.0'

datadir = join_paths(get_option('prefix'), get_option('datadir'))
includedir = join_paths(get_option('prefix'), get_option('includedir'))
libexecdir = join_paths(get_option('prefix'), get_option('libexecdir'))
sysconfdir = join_paths(get_option('prefix'), get_option('sysconfdir'))
localedir = join_paths(datadir, 'locale')

header_dir = 'libgeoclue-' + gclue_api_version
include_subdir = join_paths(includedir, header_dir)

conf = configuration_data()
conf.set_quoted('VERSION', gclue_version)
conf.set_quoted('PACKAGE_VERSION', gclue_version)
conf.set_quoted('PACKAGE_NAME', 'geoclue')
conf.set_quoted('GETTEXT_PACKAGE', 'geoclue')
conf.set_quoted('PACKAGE_TARNAME', 'geoclue')
conf.set_quoted('PACKAGE_STRING', 'geoclue ' + gclue_version)
conf.set_quoted('PACKAGE_URL', 'https://gitlab.freedesktop.org/geoclue/geoclue/wikis/home')
conf.set_quoted('PACKAGE_BUGREPORT', 'https://gitlab.freedesktop.org/geoclue/geoclue/issues/new')
conf.set_quoted('TEST_SRCDIR', meson.source_root() + '/data/')
conf.set_quoted('LOCALEDIR', localedir)
conf.set_quoted('SYSCONFDIR', sysconfdir)
conf.set10('GCLUE_USE_3G_SOURCE', get_option('3g-source'))
conf.set10('GCLUE_USE_CDMA_SOURCE', get_option('cdma-source'))
conf.set10('GCLUE_USE_MODEM_GPS_SOURCE', get_option('modem-gps-source'))
conf.set10('GCLUE_USE_NMEA_SOURCE', get_option('nmea-source'))

configure_file(output: 'config.h', configuration : conf)
configinc = include_directories('.')

gnome = import('gnome')
cc = meson.get_compiler('c')

base_deps = [ dependency('glib-2.0', version: '>= 2.44.0'),
              dependency('gio-2.0', version: '>= 2.44.0'),
              dependency('gio-unix-2.0', version: '>= 2.44.0') ]
libm = cc.find_library('m', required: false)
if libm.found()
    base_deps += [ libm ]
endif

subdir('public-api')
subdir('interface')
if get_option('enable-backend')
    subdir('src')
endif
if get_option('libgeoclue')
    subdir('libgeoclue')
endif
subdir('data')
subdir('demo')
subdir('po')

# FIXME: The D-Bus docs should not need libgeoclue but after a few hours of
# banging my head agaist gtk-doc+meson and asking on IRC, I failed to find a
# way to tell gtk-doc to link to the uninstalled public-api static library. A
# non-working solution is in 'wip/fix-srv-docs' branch.
#
# If you look into this issue, you might also want to see how we can fix the
# dependency issue that stops the user to run `ninja -C build geoclue-doc`
# before `ninja -C build`.
if get_option('gtk-doc') and get_option('libgeoclue')
  subdir('docs')
endif

systemd_unit_dir = 'N/A'

# FIXME: Remove this once meson does it itself: https://github.com/mesonbuild/meson/issues/757
summary = '''
        Geoclue @0@
        =================

        prefix:                   @1@
        c compiler:               @2@
        Systemd system unit dir:  @3@

        Backend:                  @4@
        Convenience library:      @5@
        Introspection:            @6@
        3G source:                @7@
        CDMA source:              @8@
        Modem GPS source:         @9@
        Network NMEA source:      @10@
'''.format(gclue_version,
           get_option('prefix'),
           cc.get_id(),
           systemd_unit_dir,
           get_option('enable-backend'),
           get_option('libgeoclue'),
           get_option('introspection'),
           get_option('3g-source'),
           get_option('cdma-source'),
           get_option('modem-gps-source'),
           get_option('nmea-source'))
message(summary)
