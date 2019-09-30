set (SERVICE_LIB "indicatordatetimeservice")
set (SERVICE_EXEC "indicator-datetime-service")

SET (CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c99   -g ${CXX_WARNING_ARGS} ${GCOV_FLAGS}")
SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g ${CXX_WARNING_ARGS} ${GCOV_FLAGS}")

add_definitions (-DTIMEZONE_FILE="/etc/timezone"
                 -DG_LOG_DOMAIN="Indicator-Datetime")

add_library (${SERVICE_LIB} STATIC
             actions.cpp
             actions-live.cpp
             appointment.cpp
             clock.cpp
             clock-live.cpp
             clock-watcher.cpp
             date-time.cpp
             engine-eds.cpp
             exporter.cpp
             formatter.cpp
             formatter-desktop.cpp
             locations.cpp
             locations-settings.cpp
             menu.cpp
             planner-month.cpp
             planner-range.cpp
             planner-upcoming.cpp
             settings-live.cpp
             snap.cpp
             timezone-file.cpp
             timezone-geoclue.cpp
             timezones-live.cpp
             utils.c)
include_directories (${CMAKE_SOURCE_DIR})
link_directories (${SERVICE_DEPS_LIBRARY_DIRS})

add_executable (${SERVICE_EXEC} main.cpp)
target_link_libraries (${SERVICE_EXEC} ${SERVICE_LIB} ${SERVICE_DEPS_LIBRARIES} ${GCOV_LIBS})
install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})

