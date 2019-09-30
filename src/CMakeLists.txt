add_subdirectory (backend-dbus)

add_library (libindicatorsessionservice STATIC
             actions.c
             actions.h
             guest.c
             guest.h
             recoverable-problem.c
             recoverable-problem.h
             service.c
             service.h
             users.c
             users.h)
include_directories(${SERVICE_INCLUDE_DIRS})
link_directories(${SERVICE_LIBRARY_DIRS})

set (SERVICE_EXEC "indicator-session-service")
set_property (SOURCE main.c
              APPEND PROPERTY COMPILE_DEFINITIONS
              GETTEXT_PACKAGE="${GETTEXT_PACKAGE}"
              GNOMELOCALEDIR="@CMAKE_INSTALL_FULL_LOCALEDIR@")
add_executable (${SERVICE_EXEC} main.c)
target_link_libraries (${SERVICE_EXEC} libindicatorsessionservice backenddbus ${SERVICE_LIBRARIES} ${GCOV_LIBS})
install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})

# common properties
set_property (TARGET libindicatorsessionservice ${SERVICE_EXEC}
              APPEND_STRING PROPERTY COMPILE_FLAGS
              " -g ${CC_WARNING_ARGS} ${GCOV_FLAGS}")
