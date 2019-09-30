if (CMAKE_BUILD_TYPE MATCHES coverage)
  set(GCOV_FLAGS "${GCOV_FLAGS} --coverage")
  set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}    ${GCOV_FLAGS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${GCOV_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GCOV_FLAGS}")
  set(GCOV_LIBS ${GCOV_LIBS} gcov)

  find_program(GCOVR_EXECUTABLE gcovr HINTS ${GCOVR_ROOT} "${GCOVR_ROOT}/bin")
  if (NOT GCOVR_EXECUTABLE)
    message(STATUS "Gcovr binary was not found, can not generate XML coverage info.")
  else ()
    message(STATUS "Gcovr found, can generate XML coverage info.")
    add_custom_target (coverage-xml
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMAND "${GCOVR_EXECUTABLE}" --exclude="test.*" -x -r "${CMAKE_SOURCE_DIR}" 
      --object-directory=${CMAKE_BINARY_DIR} -o coverage.xml)
  endif()

  find_program(LCOV_EXECUTABLE lcov HINTS ${LCOV_ROOT} "${GCOVR_ROOT}/bin")
  find_program(GENHTML_EXECUTABLE genhtml HINTS ${GENHTML_ROOT})
  if (NOT LCOV_EXECUTABLE)
    message(STATUS "Lcov binary was not found, can not generate HTML coverage info.")
  else ()
    if(NOT GENHTML_EXECUTABLE)
      message(STATUS "Genthml binary not found, can not generate HTML coverage info.")
    else()
      message(STATUS "Lcov and genhtml found, can generate HTML coverage info.")
      add_custom_target (coverage-html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND "${CMAKE_CTEST_COMMAND}" --force-new-ctest-process --verbose
        COMMAND "${LCOV_EXECUTABLE}" --directory ${CMAKE_BINARY_DIR} --capture | ${CMAKE_SOURCE_DIR}/trim-lcov.py > dconf-lcov.info
        COMMAND "${LCOV_EXECUTABLE}" -r dconf-lcov.info /usr/include/\\*  -o nosys-lcov.info
        COMMAND LANG=C "${GENHTML_EXECUTABLE}" --prefix ${CMAKE_BINARY_DIR} --output-directory lcov-html --legend --show-details nosys-lcov.info
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "file://${CMAKE_BINARY_DIR}/lcov-html/index.html"
        COMMAND ${CMAKE_COMMAND} -E echo "")
        #COMMAND "${LCOV_EXECUTABLE}" --directory ${CMAKE_BINARY_DIR} --capture --output-file coverage.info --no-checksum
        #COMMAND "${GENHTML_EXECUTABLE}" --prefix ${CMAKE_BINARY_DIR} --output-directory coveragereport --title "Code Coverage" --legend --show-details coverage.info
        #COMMAND ${CMAKE_COMMAND} -E echo "\\#define foo \\\"bar\\\"" 
        #)
    endif()
  endif()
endif()


	#$(MAKE) $(AM_MAKEFLAGS) check
	#lcov --directory $(top_builddir) --capture --test-name dconf | $(top_srcdir)/trim-lcov.py > dconf-lcov.info
	#LANG=C genhtml --prefix $(top_builddir) --output-directory lcov-html --legend --show-details dconf-lcov.info
	#@echo
	#@echo "     file://$(abs_top_builddir)/lcov-html/index.html"
	#@echo
