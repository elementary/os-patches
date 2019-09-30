# build libgtest
add_library (gtest STATIC 
             ${GTEST_SOURCE_DIR}/gtest-all.cc 
             ${GTEST_SOURCE_DIR}/gtest_main.cc)
set_target_properties (gtest PROPERTIES INCLUDE_DIRECTORIES
                       ${GTEST_INCLUDE_DIR})

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -g ${CC_WARNING_ARGS}")

# build desktopmock
add_library (desktopmock STATIC
             mock-accounts.cc
             mock-accounts.h
             mock-login1-manager.cc
             mock-login1-manager.h
             mock-login1-seat.cc
             mock-login1-seat.h
             mock-display-manager-seat.cc
             mock-display-manager-seat.h
             mock-end-session-dialog.cc
             mock-end-session-dialog.h
             mock-object.cc
             mock-object.h
             mock-screen-saver.cc
             mock-screen-saver.h
             mock-unity-session.cc
             mock-unity-session.h
             mock-session-manager.cc
             mock-session-manager.h
             mock-user.cc
             mock-user.h
             mock-webcredentials.cc
             mock-webcredentials.h)

include_directories (${SERVICE_INCLUDE_DIRS})
include_directories (${CMAKE_SOURCE_DIR}/src)
include_directories (${CMAKE_BINARY_DIR}/src)
include_directories (${CMAKE_SOURCE_DIR}/tests)

# test the Actions class
add_executable (test-actions
                test-actions.cc)
add_test (test-actions test-actions)
set_tests_properties (test-actions PROPERTIES COMPILE_FLAGS "${SERVICE_CFLAGS}")
target_link_libraries (test-actions desktopmock backenddbus libindicatorsessionservice gtest ${SERVICE_LDFLAGS} ${GTEST_LIBS} ${GCOV_LIBS})

# test the Guest class
add_executable (test-guest
                test-guest.cc)
add_test (test-guest test-guest)
set_tests_properties (test-guest PROPERTIES COMPILE_FLAGS "${SERVICE_CFLAGS}")
target_link_libraries (test-guest desktopmock backenddbus libindicatorsessionservice gtest ${SERVICE_LDFLAGS} ${GTEST_LIBS} ${GCOV_LIBS})

# test the Users class
add_executable (test-users
                test-users.cc)
add_test (test-users test-users)
set_tests_properties (test-users PROPERTIES COMPILE_FLAGS "${SERVICE_CFLAGS}")
target_link_libraries (test-users desktopmock backenddbus libindicatorsessionservice gtest ${SERVICE_LDFLAGS} ${GTEST_LIBS} ${GCOV_LIBS})



