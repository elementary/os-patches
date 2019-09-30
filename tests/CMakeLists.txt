# build the necessary schemas
set_directory_properties (PROPERTIES
                          ADDITIONAL_MAKE_CLEAN_FILES gschemas.compiled)
set_source_files_properties (gschemas.compiled GENERATED)

# GSettings:
# compile the schemas our tests use (indicator-session's, lockdown, media-keys)
# into a gschemas.compiled file in this directory, and help the tests to find
# that file by setting -DSCHEMA_DIR
set (SCHEMA_DIR ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DSCHEMA_DIR="${SCHEMA_DIR}")
execute_process (COMMAND ${PKG_CONFIG_EXECUTABLE} gio-2.0 --variable glib_compile_schemas
                 OUTPUT_VARIABLE COMPILE_SCHEMA_EXECUTABLE
                 OUTPUT_STRIP_TRAILING_WHITESPACE)
add_custom_command (OUTPUT gschemas.compiled
                    DEPENDS ${CMAKE_BINARY_DIR}/data/com.canonical.indicator.session.gschema.xml
                            ${CMAKE_SOURCE_DIR}/tests/com.canonical.indicator.session.backendmock.gschema.xml
                            ${CMAKE_SOURCE_DIR}/tests/org.gnome.desktop.lockdown.gschema.xml
                            ${CMAKE_SOURCE_DIR}/tests/org.gnome.settings-daemon.plugins.media-keys.gschema.xml
                    COMMAND cp -f ${CMAKE_BINARY_DIR}/data/*gschema.xml ${SCHEMA_DIR}
                    COMMAND cp -f ${CMAKE_SOURCE_DIR}/tests/*gschema.xml ${SCHEMA_DIR}
                    COMMAND ${COMPILE_SCHEMA_EXECUTABLE} ${SCHEMA_DIR})

# DBus Activation
configure_file (indicator-session.service.in indicator-session.service)
add_definitions(-DINDICATOR_SERVICE_DIR="${CMAKE_CURRENT_BINARY_DIR}")

# look for hearder in our src dir, and also in the directories where we autogenerate files...
include_directories (${CMAKE_SOURCE_DIR}/src)
include_directories (${CMAKE_CURRENT_BINARY_DIR} ${SERVICE_INCLUDE_DIRS})

# backendmock
add_library (backendmock STATIC
             backend-mock-actions.c 
             backend-mock-actions.h
             backend-mock.c
             backend-mock.h
             backend-mock-guest.c
             backend-mock-guest.h
             backend-mock-users.c
             backend-mock-users.h)
set_target_properties (backendmock PROPERTIES COMPILE_FLAGS " ${CC_WARNING_ARGS} -g")

# test-service
add_executable (test-service
                test-service.cc
                gschemas.compiled)
set_target_properties (test-service PROPERTIES COMPILE_FLAGS " ${CC_WARNING_ARGS} -std=c++0x -g")
add_test (test-service test-service)
add_dependencies (test-service libindicatorsessionservice backendmock)
target_link_libraries (test-service libindicatorsessionservice backendmock gtest ${SERVICE_LIBRARIES} ${GTEST_LIBS})

add_subdirectory (backend-dbus)
