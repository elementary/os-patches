
include(UseGSettings)

###########################
# Indicator service
###########################

set(
  INDICATOR_DIR
  "${CMAKE_INSTALL_DATADIR}/unity/indicators"
  CACHE FILEPATH "Indicator directory"
)

install(
  FILES "com.canonical.indicator.sound"
  DESTINATION "${INDICATOR_DIR}"
)

###########################
# Upstart Job
###########################

set(
  INDICATOR_SOUND_CONF
  "${CMAKE_CURRENT_BINARY_DIR}/indicator-sound.conf"
)

configure_file(
  "indicator-sound.conf.in"
  ${INDICATOR_SOUND_CONF}
  @ONLY
)

install(
  FILES "${INDICATOR_SOUND_CONF}"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/upstart/sessions/"
)

###########################
# XDG Autostart
###########################

set(
  INDICATOR_SOUND_XDG_AUTOSTART
  "${CMAKE_CURRENT_BINARY_DIR}/indicator-sound.desktop"
)

configure_file(
  "indicator-sound.desktop.in"
  ${INDICATOR_SOUND_XDG_AUTOSTART}
  @ONLY
)

install(
  FILES "${INDICATOR_SOUND_XDG_AUTOSTART}"
  DESTINATION "/etc/xdg/autostart"
)

###########################
# Upstart XDG Autostart Override
###########################

set(
  INDICATOR_SOUND_UPSTART_XDG_AUTOSTART
  "${CMAKE_CURRENT_BINARY_DIR}/indicator-sound.upstart.desktop"
)

configure_file(
  "indicator-sound.upstart.desktop.in"
  ${INDICATOR_SOUND_UPSTART_XDG_AUTOSTART}
  @ONLY
)

install(
  FILES "${INDICATOR_SOUND_UPSTART_XDG_AUTOSTART}"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/upstart/xdg/autostart"
  RENAME "indicator-sound.desktop"
)

###########################
# GSettings
###########################

add_schema ("com.canonical.indicator.sound.gschema.xml")

###########################
# Accounts Service
###########################


set(POLKIT_LIB_DIR "${CMAKE_INSTALL_LOCALSTATEDIR}/lib/polkit-1")
set(POLKIT_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/polkit-1")
set(DBUS_IFACE_DIR "${CMAKE_INSTALL_PREFIX}/share/dbus-1/interfaces")
set(ACCOUNTS_IFACE_DIR "${CMAKE_INSTALL_PREFIX}/share/accountsservice/interfaces")

install(FILES com.canonical.indicator.sound.AccountsService.xml
	DESTINATION "${DBUS_IFACE_DIR}"
)

# Create accountsservice symlink for above dbus interface
install(CODE "
	execute_process(COMMAND mkdir -p \"\$ENV{DESTDIR}${ACCOUNTS_IFACE_DIR}\")
	execute_process(COMMAND ln -sf ../../dbus-1/interfaces/com.canonical.indicator.sound.AccountsService.xml \"\$ENV{DESTDIR}${ACCOUNTS_IFACE_DIR}\")
")

install(FILES com.canonical.indicator.sound.AccountsService.policy
DESTINATION "${POLKIT_DATA_DIR}/actions"
)

install(FILES 50-com.canonical.indicator.sound.AccountsService.pkla
DESTINATION "${POLKIT_LIB_DIR}/localauthority/10-vendor.d"
)




