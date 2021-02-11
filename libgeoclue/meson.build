# Client interface
gclue_client = gnome.gdbus_codegen(
    'gclue-client',
    '../interface/org.freedesktop.GeoClue2.Client.xml',
    interface_prefix: 'org.freedesktop.GeoClue2.',
    namespace: 'GClue',
    install_header: true,
    install_dir: include_subdir)

# Location interface
gclue_location = gnome.gdbus_codegen(
    'gclue-location',
    '../interface/org.freedesktop.GeoClue2.Location.xml',
    interface_prefix: 'org.freedesktop.GeoClue2.',
    namespace: 'GClue',
    install_header: true,
    install_dir: include_subdir)

# Manager interface
gclue_manager = gnome.gdbus_codegen(
    'gclue-manager',
    '../interface/org.freedesktop.GeoClue2.Manager.xml',
    interface_prefix: 'org.freedesktop.GeoClue2.',
    namespace: 'GClue',
    install_header: true,
    install_dir: include_subdir)

libgeoclue_sources = files('gclue-helpers.c', 'gclue-simple.c')
libgeoclue_sources += gclue_client[0]
libgeoclue_sources += gclue_location[0]
libgeoclue_sources += gclue_manager[0]
libgeoclue_sources += libgeoclue_public_api_gen_sources[1]

libgeoclue_headers = files('geoclue.h', 'gclue-helpers.h', 'gclue-simple.h')

install_headers(libgeoclue_headers, subdir: header_dir)

libgeoclue_headers += gclue_client[1]
libgeoclue_headers += gclue_location[1]
libgeoclue_headers += gclue_manager[1]

c_args = [ '-DG_LOG_DOMAIN="Geoclue"' ]
include_dirs = [ libgeoclue_public_api_inc, include_directories('.', '..') ]
link_whole = [ libgeoclue_public_api ]
libgeoclue = library('geoclue-2',
                     libgeoclue_sources,
                     libgeoclue_headers,
                     include_directories: include_dirs,
                     dependencies: base_deps,
                     link_whole: link_whole,
                     soversion: '0',
                     version: '0.0.0',
                     install: true)

libgeoclue_dep = declare_dependency(link_with: libgeoclue,
                                    include_directories: include_dirs,
                                    dependencies: base_deps,
                                    sources: [ libgeoclue_sources, libgeoclue_headers ])

gir = find_program('g-ir-scanner', required: false)
vapigen = find_program('vapigen', required: false)
enable_gir = get_option('introspection')
pkg_requirements = ['glib-2.0', 'gio-2.0', 'gio-unix-2.0']
gir_sources = [ libgeoclue_sources,
                libgeoclue_headers,
                libgeoclue_public_api_gen_sources[1] ]

if gir.found() and enable_gir
  geo_gir = gnome.generate_gir(libgeoclue,
                               sources: gir_sources,
                               namespace: 'Geoclue',
                               nsversion: gclue_api_version,
                               identifier_prefix: 'GClue',
                               symbol_prefix: 'gclue',
                               export_packages: 'libgeoclue-' + gclue_api_version,
                               dependencies: [libgeoclue_dep] + base_deps,
                               includes: [ 'GObject-2.0', 'Gio-2.0' ],
                               install: true,
                               header: 'geoclue.h',
                               extra_args: [ '--quiet' ])
  if vapigen.found()
    gnome.generate_vapi('libgeoclue-' + gclue_api_version,
                        sources: geo_gir[0],
                        packages: pkg_requirements,
                        install: true)
  endif
endif

description = 'A convenience library to interact with Geoclue service'
pkgconf = import('pkgconfig')
pkgconf.generate(version: gclue_version,
                 name: 'Geoclue Client Library',
                 description: description,
                 filebase: 'libgeoclue-' + gclue_api_version,
                 libraries: libgeoclue,
                 subdirs: [ header_dir ],
                 requires: pkg_requirements)

