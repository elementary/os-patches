cmake_minimum_required(VERSION 2.6)
if(POLICY CMP0011)
  cmake_policy(SET CMP0011 NEW)
endif(POLICY CMP0011)

find_program(GDBUS_CODEGEN NAMES gdbus-codegen DOC "gdbus-codegen executable")
if(NOT GDBUS_CODEGEN)
  message(FATAL_ERROR "Excutable gdbus-codegen not found")
endif()

macro(add_gdbus_codegen outfiles name prefix service_xml)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.h" "${CMAKE_CURRENT_BINARY_DIR}/${name}.c"
    COMMAND "${GDBUS_CODEGEN}"
        --interface-prefix "${prefix}"
        --generate-c-code "${name}"
        "${service_xml}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${ARGN} "${service_xml}"
  )
  list(APPEND ${outfiles} "${CMAKE_CURRENT_BINARY_DIR}/${name}.c")
endmacro(add_gdbus_codegen)

macro(add_gdbus_codegen_with_namespace outfiles name prefix namespace service_xml)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.h" "${CMAKE_CURRENT_BINARY_DIR}/${name}.c"
    COMMAND "${GDBUS_CODEGEN}"
        --interface-prefix "${prefix}"
        --generate-c-code "${name}"
        --c-namespace "${namespace}"
        "${service_xml}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${ARGN} "${service_xml}"
  )
  list(APPEND ${outfiles} "${CMAKE_CURRENT_BINARY_DIR}/${name}.c")
endmacro(add_gdbus_codegen_with_namespace)
