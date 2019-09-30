
###########################
# Google Test
###########################

include_directories(${GTEST_INCLUDE_DIR})

add_library (gtest STATIC
             ${GTEST_SOURCE_DIR}/gtest-all.cc
             ${GTEST_SOURCE_DIR}/gtest_main.cc)
target_link_libraries(gtest ${GTEST_LIBS})

###########################
# Vala Mocks
###########################

set(VALA_MOCKS_HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/vala-mocks.h")
set(VALA_MOCKS_SYMBOLS_PATH "${CMAKE_CURRENT_BINARY_DIR}/vala-mocks.def")

vala_init(vala-mocks
	DEPENDS
		indicator-sound-service-lib
	PACKAGES
		config
		gio-2.0
		gio-unix-2.0
		libxml-2.0
		libpulse
		libpulse-mainloop-glib
		libnotify
		accounts-service
		indicator-sound-service
	OPTIONS
		--ccode
		--thread
		--vapidir=${CMAKE_BINARY_DIR}/src/
		--vapidir=${CMAKE_SOURCE_DIR}/vapi/
		--vapidir=.
)

vala_add(vala-mocks
	media-player-mock.vala
)

vala_finish(vala-mocks
	SOURCES
		vala_mocks_VALA_SOURCES
	OUTPUTS
		vala_mocks_VALA_C
	GENERATE_HEADER
		${VALA_MOCKS_HEADER_PATH}
	GENERATE_SYMBOLS
		${VALA_MOCKS_SYMBOLS_PATH}
)

set_source_files_properties(
	${vala_mocks_VALA_SOURCES}
	PROPERTIES
	HEADER_FILE_ONLY TRUE
)

set(
	VALA_MOCKS_SOURCES
	${vala_mocks_VALA_SOURCES}
	${vala_mocks_VALA_C}
	${VALA_MOCKS_SYMBOLS_PATH}
)

add_definitions(
	-Wno-unused-but-set-variable
)

add_library(
	vala-mocks-lib STATIC
	${VALA_MOCKS_SOURCES}
)

target_link_libraries(
	vala-mocks-lib
	indicator-sound-service-lib
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

###########################
# Name Watch Test
###########################

include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable (name-watch-test name-watch-test.cc ${CMAKE_SOURCE_DIR}/src/bus-watch-namespace.c)
target_link_libraries (name-watch-test gtest ${SOUNDSERVICE_LIBRARIES})
add_test(name-watch-test name-watch-test)

###########################
# Accounts Service User
###########################

include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable (accounts-service-user-test accounts-service-user.cc)
target_link_libraries (
	accounts-service-user-test
	indicator-sound-service-lib
	vala-mocks-lib
	gtest
	${SOUNDSERVICE_LIBRARIES}
	${TEST_LIBRARIES}
)

# Split tests to work around libaccountservice sucking
add_test(accounts-service-user-test-basic
	accounts-service-user-test  --gtest_filter=AccountsServiceUserTest.BasicObject
)

add_test(accounts-service-user-test-player
	accounts-service-user-test  --gtest_filter=AccountsServiceUserTest.SetMediaPlayer
)

###########################
# Sound Menu
###########################

include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable (sound-menu-test sound-menu.cc)
target_link_libraries (
	sound-menu-test
	indicator-sound-service-lib
	vala-mocks-lib
	gtest
	${SOUNDSERVICE_LIBRARIES}
	${TEST_LIBRARIES}
)

add_test(sound-menu-test sound-menu-test)

###########################
# Accounts Service User
###########################

include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable (media-player-user-test media-player-user.cc)
target_link_libraries (
	media-player-user-test
	indicator-sound-service-lib
	vala-mocks-lib
	gtest
	${SOUNDSERVICE_LIBRARIES}
	${TEST_LIBRARIES}
)

# Split tests to work around libaccountservice sucking
add_test(media-player-user-test-basic
	media-player-user-test  --gtest_filter=MediaPlayerUserTest.BasicObject
)
add_test(media-player-user-test-dataset
	media-player-user-test  --gtest_filter=MediaPlayerUserTest.DataSet
)
add_test(media-player-user-test-timeout
	media-player-user-test  --gtest_filter=MediaPlayerUserTest.TimeoutTest
)

###########################
# Greeter List
###########################

include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable (greeter-list-test greeter-list.cc)
target_link_libraries (
	greeter-list-test
	indicator-sound-service-lib
	vala-mocks-lib
	gtest
	${SOUNDSERVICE_LIBRARIES}
	${TEST_LIBRARIES}
)

# Split tests to work around libaccountservice sucking
add_test(greeter-list-test-basic
	greeter-list-test  --gtest_filter=GreeterListTest.BasicObject
)
add_test(greeter-list-test-iterator
	greeter-list-test  --gtest_filter=GreeterListTest.BasicIterator
)

