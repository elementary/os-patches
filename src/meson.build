libgtkcloudproviders_headers = [
  'gtkcloudprovidermanager.h',
  'gtkcloudprovider.h'
]

libgtkcloudproviders_sources = [
  'gtkcloudprovidermanager.c',
  'gtkcloudprovider.c'
]

libgtkcloudproviders_deps = [glib, gtk]

libgtkcloudproviders = shared_library ('gtkcloudproviders',
                                        libgtkcloudproviders_sources,
                                        dependencies: libgtkcloudproviders_deps,
                                        version: meson.project_version (),
                                        install: true)

install_headers( libgtkcloudproviders_headers )

