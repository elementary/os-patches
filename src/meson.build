geoclue_deps = base_deps + [ dependency('json-glib-1.0', version: '>= 0.14.0'),
                             dependency('libsoup-2.4', version: '>= 2.42.0') ]

sources = [ libgeoclue_public_api_gen_sources[1],
            geoclue_iface_sources,
            wpa_supplicant_sources,
            compass_iface_sources ]

sources += gnome.genmarshal('gclue-marshal',
                            prefix: 'gclue_marshal',
                            sources: ['gclue-marshal.list'])

include_dirs = [ configinc,
                 libgeoclue_public_api_inc,
                 include_directories('..') ]

sources += [ 'gclue-main.c',
             'gclue-3g-tower.h',
             'gclue-client-info.h', 'gclue-client-info.c',
             'gclue-compass.h', 'gclue-compass.c',
             'gclue-config.h', 'gclue-config.c',
             'gclue-error.h', 'gclue-error.c',
             'gclue-location-source.h', 'gclue-location-source.c',
             'gclue-locator.h', 'gclue-locator.c',
             'gclue-service-manager.h', 'gclue-service-manager.c',
             'gclue-service-client.h', 'gclue-service-client.c',
             'gclue-service-location.h', 'gclue-service-location.c',
             'gclue-web-source.c', 'gclue-web-source.h',
             'gclue-wifi.h', 'gclue-wifi.c',
             'gclue-mozilla.h', 'gclue-mozilla.c',
             'gclue-min-uint.h', 'gclue-min-uint.c',
             'gclue-location.h', 'gclue-location.c' ]

if get_option('3g-source') or get_option('cdma-source') or get_option('modem-gps-source')
    geoclue_deps += [ dependency('mm-glib', version: '>= 1.6') ]
    sources += [ 'gclue-modem.c',
				 'gclue-modem.h',
				 'gclue-modem-manager.c',
				 'gclue-modem-manager.h' ]
endif

if get_option('3g-source')
    sources += [ 'gclue-3g.c', 'gclue-3g.h' ]
endif

if get_option('cdma-source')
    sources += [ 'gclue-cdma.c', 'gclue-cdma.h' ]
endif

if get_option('modem-gps-source')
    sources += [ 'gclue-modem-gps.c', 'gclue-modem-gps.h' ]
endif

if get_option('nmea-source')
    geoclue_deps += [ dependency('avahi-client', version: '>= 0.6.10'),
                      dependency('avahi-glib', version: '>= 0.6.10') ]
    sources += [ 'gclue-nmea-source.h', 'gclue-nmea-source.c' ]
endif

c_args = [ '-DG_LOG_DOMAIN="Geoclue"' ]
link_with = [ libgeoclue_public_api ]
executable('geoclue',
           sources,
           link_with: link_with,
           include_directories: include_dirs,
           c_args: c_args,
           dependencies: geoclue_deps,
           install: true,
           install_dir: libexecdir)

dbus_interface = join_paths(dbus_interface_dir, 'org.freedesktop.GeoClue2.xml')
agent_dbus_interface = join_paths(dbus_interface_dir, 'org.freedesktop.GeoClue2.Agent.xml')
pkgconf = import('pkgconfig')
pkgconf.generate(version: gclue_version,
                 name: 'Geoclue',
                 description: 'The Geoinformation Service',
                 filebase: 'geoclue-' + gclue_api_version,
                 variables: [ 'apiversion=' + gclue_api_version,
                              'dbus_interface=' + dbus_interface,
                              'agent_dbus_interface=' + agent_dbus_interface ])
