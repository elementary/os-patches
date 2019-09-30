if (CMAKE_BUILD_TYPE MATCHES coverage)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} --coverage")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")

  find_program(GCOVR_EXECUTABLE gcovr HINTS ${GCOVR_ROOT} "${GCOVR_ROOT}/bin")
  if (NOT GCOVR_EXECUTABLE)
    message(STATUS "Gcovr binary was not found, can not generate XML coverage info.")
  else ()
    message(STATUS "Gcovr found, can generate XML coverage info.")
    add_custom_target (coverage-xml
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMAND "${GCOVR_EXECUTABLE}" --exclude="test.*" --exclude="obj.*" -x -r "${CMAKE_SOURCE_DIR}" 
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
        COMMAND "${LCOV_EXECUTABLE}" --capture --output-file "${CMAKE_BINARY_DIR}/coverage.info" --no-checksum --directory "${CMAKE_BINARY_DIR}"
        COMMAND "${LCOV_EXECUTABLE}" --remove "${CMAKE_BINARY_DIR}/coverage.info" '/usr/*' --output-file "${CMAKE_BINARY_DIR}/coverage.info" 
        COMMAND "${LCOV_EXECUTABLE}" --remove "${CMAKE_BINARY_DIR}/coverage.info" '${CMAKE_BINARY_DIR}/*' --output-file "${CMAKE_BINARY_DIR}/coverage.info" 
        COMMAND "${LCOV_EXECUTABLE}" --remove "${CMAKE_BINARY_DIR}/coverage.info" '${CMAKE_SOURCE_DIR}/tests/*' --output-file "${CMAKE_BINARY_DIR}/coverage.info" 
        COMMAND "${GENHTML_EXECUTABLE}" --prefix "${CMAKE_BINARY_DIR}" --output-directory coveragereport --title "Code Coverage" --legend --show-details coverage.info
        )
    endif()
  endif()
endif()
