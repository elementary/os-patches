##
##  GSettings schema
##

include (UseGSettings)
set (SCHEMA_NAME "com.canonical.indicator.datetime.gschema.xml")
set (SCHEMA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${SCHEMA_NAME}")
add_schema (${SCHEMA_FILE})


##
##  Upstart Job File
##

# where to install
set (UPSTART_JOB_DIR "${CMAKE_INSTALL_FULL_DATADIR}/upstart/sessions")
message (STATUS "${UPSTART_JOB_DIR} is the Upstart Job File install dir")

set (UPSTART_JOB_NAME "${CMAKE_PROJECT_NAME}.conf")
set (UPSTART_JOB_FILE "${CMAKE_CURRENT_BINARY_DIR}/${UPSTART_JOB_NAME}")
set (UPSTART_JOB_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${UPSTART_JOB_NAME}.in")

# build it
set (pkglibexecdir "${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}")
configure_file ("${UPSTART_JOB_FILE_IN}" "${UPSTART_JOB_FILE}")

# install it
install (FILES "${UPSTART_JOB_FILE}"
         DESTINATION "${UPSTART_JOB_DIR}")

##
##  XDG Autostart File
##

# where to install
set (XDG_AUTOSTART_DIR "/etc/xdg/autostart")
message (STATUS "${XDG_AUTOSTART_DIR} is the DBus Service File install dir")

set (XDG_AUTOSTART_NAME "${CMAKE_PROJECT_NAME}.desktop")
set (XDG_AUTOSTART_FILE "${CMAKE_CURRENT_BINARY_DIR}/${XDG_AUTOSTART_NAME}")
set (XDG_AUTOSTART_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${XDG_AUTOSTART_NAME}.in")

# build it
set (pkglibexecdir "${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}")
configure_file ("${XDG_AUTOSTART_FILE_IN}" "${XDG_AUTOSTART_FILE}")

# install it
install (FILES "${XDG_AUTOSTART_FILE}"
         DESTINATION "${XDG_AUTOSTART_DIR}")

##
##  Upstart XDG Autostart Override
##

# where to install
set (UPSTART_XDG_AUTOSTART_DIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/upstart/xdg/autostart")
message (STATUS "${UPSTART_XDG_AUTOSTART_DIR} is the Upstart XDG autostart override dir")

set (UPSTART_XDG_AUTOSTART_NAME "${CMAKE_PROJECT_NAME}.upstart.desktop")
set (UPSTART_XDG_AUTOSTART_FILE "${CMAKE_CURRENT_BINARY_DIR}/${UPSTART_XDG_AUTOSTART_NAME}")
set (UPSTART_XDG_AUTOSTART_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${UPSTART_XDG_AUTOSTART_NAME}.in")

# build it
set (pkglibexecdir "${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}")
configure_file ("${UPSTART_XDG_AUTOSTART_FILE_IN}" "${UPSTART_XDG_AUTOSTART_FILE}")

# install it
install (FILES "${UPSTART_XDG_AUTOSTART_FILE}"
         DESTINATION "${UPSTART_XDG_AUTOSTART_DIR}"
         RENAME "${XDG_AUTOSTART_NAME}")

##
##  Unity Indicator File
##

# where to install
set (UNITY_INDICATOR_DIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/unity/indicators")
message (STATUS "${UNITY_INDICATOR_DIR} is the Unity Indicator install dir")

set (UNITY_INDICATOR_NAME "com.canonical.indicator.datetime")
set (UNITY_INDICATOR_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${UNITY_INDICATOR_NAME}")

install (FILES "${UNITY_INDICATOR_FILE}"
         DESTINATION "${UNITY_INDICATOR_DIR}")
