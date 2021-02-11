dbus_interface_dir = join_paths(datadir, 'dbus-1', 'interfaces')

geoclue_iface_sources = []

# Client interface
client_interface_xml = 'org.freedesktop.GeoClue2.Client.xml'
geoclue_iface_sources += gnome.gdbus_codegen(
    'gclue-client-interface',
    client_interface_xml,
    interface_prefix: 'org.freedesktop.GeoClue2.',
    namespace: 'GClueDBus',
    docbook: 'docs')
# Location interface
location_interface_xml = 'org.freedesktop.GeoClue2.Location.xml'
geoclue_iface_sources += gnome.gdbus_codegen(
    'gclue-location-interface',
    location_interface_xml,
    interface_prefix: 'org.freedesktop.GeoClue2.',
    namespace: 'GClueDBus',
    docbook: 'docs')
# Manager interface
manager_interface_xml = 'org.freedesktop.GeoClue2.Manager.xml'
geoclue_iface_sources += gnome.gdbus_codegen(
    'gclue-manager-interface',
    manager_interface_xml,
    interface_prefix: 'org.freedesktop.GeoClue2.',
    namespace: 'GClueDBus',
    docbook: 'docs')

annotations = [[ 'fi.w1.wpa_supplicant1',
                 'org.gtk.GDBus.C.Name',
                 'WPA_Supplicant' ],
               [ 'fi.w1.wpa_supplicant1.Interface',
                 'org.gtk.GDBus.C.Name',
                 'WPA_Interface' ],
               [ 'fi.w1.wpa_supplicant1.BSS',
                 'org.gtk.GDBus.C.Name',
                 'WPA_BSS' ],
               [ 'fi.w1.wpa_supplicant1.BSS:SSID',
                 'org.gtk.GDBus.C.ForceGVariant',
                 'whatever' ],
               [ 'fi.w1.wpa_supplicant1.BSS:BSSID',
                 'org.gtk.GDBus.C.ForceGVariant',
                 'whatever' ],
               [ 'fi.w1.wpa_supplicant1.Interface::BSSAdded',
                 'org.gtk.GDBus.C.Name',
                 'BSS_Added' ],
               [ 'fi.w1.wpa_supplicant1.Interface::BSSRemoved',
                 'org.gtk.GDBus.C.Name',
                 'BSS_Removed' ]]

geoclue_agent_sources = gnome.gdbus_codegen(
    'geoclue-agent-interface',
    'org.freedesktop.GeoClue2.Agent.xml',
    interface_prefix: 'org.freedesktop.GeoClue2.',
    namespace: 'GClue',
    docbook: 'docs')
geoclue_iface_sources += geoclue_agent_sources

#libgeoclue_iface_inc = include_directories('.', '../..')

if get_option('enable-backend')
  wpa_supplicant_sources = gnome.gdbus_codegen(
      'wpa_supplicant-interface',
      'fi.w1.wpa_supplicant1.xml',
      interface_prefix: 'fi.w1.wpa_supplicant1.',
      annotations: annotations)

  compass_iface_sources = gnome.gdbus_codegen(
      'compass-interface',
      'net.hadess.SensorProxy.xml',
      interface_prefix: 'net.hadess.SensorProxy')
endif

install_data('org.freedesktop.GeoClue2.Agent.xml',
             install_dir: dbus_interface_dir)

interface_files = [ location_interface_xml,
                    client_interface_xml,
                    manager_interface_xml ]
# Provide a single interface file too for backwards compatiblity.
# At least gnome-settings-daemon currently relies on that.
unified_interface_xml = 'org.freedesktop.GeoClue2.xml'
python = find_program('python3')
create_interface = find_program('create-unified-xml.py')
cmd = [python, create_interface, '@OUTPUT@', '@INPUT@' ]

custom_target('gclue-xml-interface',
              input: interface_files,
              output: unified_interface_xml,
              install: true,
              install_dir: dbus_interface_dir,
              command: cmd)
install_data(interface_files,
             install_dir: dbus_interface_dir)
