
###########################
# Vala Generation
###########################

set(HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/indicator-sound-service.h")
set(SYMBOLS_PATH "${CMAKE_CURRENT_BINARY_DIR}/indicator-sound-service.def")
set(VAPI_PATH "${CMAKE_CURRENT_BINARY_DIR}/indicator-sound-service.vapi")

vapi_gen(accounts-service
	LIBRARY
		accounts-service
	PACKAGES
		gio-2.0
	INPUT
		/usr/share/gir-1.0/AccountsService-1.0.gir
)

vala_init(indicator-sound-service
  DEPENDS
    accounts-service
  PACKAGES
    config
    gio-2.0
    gio-unix-2.0
    libxml-2.0
    libpulse
    libpulse-mainloop-glib
    libnotify
    accounts-service
  OPTIONS
    --ccode
    --thread
    --vapidir=${CMAKE_SOURCE_DIR}/vapi/
    --vapidir=.
    --pkg=url-dispatcher
    --pkg=bus-watcher
)

vala_add(indicator-sound-service
  service.vala
  DEPENDS
    sound-menu
    volume-control
    media-player
    media-player-list
    mpris2-interfaces
    accounts-service-user
)
vala_add(indicator-sound-service
  volume-control.vala
)
vala_add(indicator-sound-service
  media-player.vala
)
vala_add(indicator-sound-service
  media-player-mpris.vala
  DEPENDS
    media-player
    mpris2-interfaces
)
vala_add(indicator-sound-service
  media-player-user.vala
  DEPENDS
    media-player
	accounts-service-sound-settings
    greeter-broadcast
)
vala_add(indicator-sound-service
  media-player-list.vala
  DEPENDS
    media-player
)
vala_add(indicator-sound-service
  media-player-list-mpris.vala
  DEPENDS
    media-player-list
    media-player
    media-player-mpris
    mpris2-interfaces
)
vala_add(indicator-sound-service
  media-player-list-greeter.vala
  DEPENDS
    media-player-list
    media-player-user
    media-player
)
vala_add(indicator-sound-service
  mpris2-interfaces.vala
)
vala_add(indicator-sound-service
  freedesktop-interfaces.vala
)
vala_add(indicator-sound-service
  sound-menu.vala
  DEPENDS
    media-player
)
vala_add(indicator-sound-service
  accounts-service-user.vala
  DEPENDS
    media-player
    mpris2-interfaces
    accounts-service-sound-settings
    greeter-broadcast
)
vala_add(indicator-sound-service
  accounts-service-sound-settings.vala
)
vala_add(indicator-sound-service
  greeter-broadcast.vala
)

vala_finish(indicator-sound-service
  SOURCES
    project_VALA_SOURCES
  OUTPUTS
    project_VALA_C
  GENERATE_HEADER
    ${HEADER_PATH}
  GENERATE_SYMBOLS
    ${SYMBOLS_PATH}
  GENERATE_VAPI
    ${VAPI_PATH}
)

set_source_files_properties(
  ${project_VALA_SOURCES}
  PROPERTIES
  HEADER_FILE_ONLY TRUE
)

set(
  INDICATOR_SOUND_SOURCES
  ${project_VALA_SOURCES}
  ${project_VALA_C}
  bus-watch-namespace.c
  ${SYMBOLS_PATH}
)

###########################
# Lib
###########################

add_definitions(
  -w
)

add_library(
	indicator-sound-service-lib STATIC
	${INDICATOR_SOUND_SOURCES}
)

target_link_libraries(
  indicator-sound-service-lib
  ${PULSEAUDIO_LIBRARIES}
  ${SOUNDSERVICE_LIBRARIES}
)

###########################
# Executable
###########################

include_directories(${CMAKE_BINARY_DIR})

add_executable(
  indicator-sound-service-bin
  main.c
)

set_target_properties(
  indicator-sound-service-bin
  PROPERTIES
  OUTPUT_NAME "indicator-sound-service"
)

target_link_libraries(
  indicator-sound-service-bin
  indicator-sound-service-lib
)

###########################
# Installation
###########################

install(
  TARGETS indicator-sound-service-bin
  RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/indicator-sound/
)

