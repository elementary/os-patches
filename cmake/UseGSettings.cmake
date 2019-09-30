# GSettings.cmake, CMake macros written for Marlin, feel free to re-use them.

macro(add_schema SCHEMA_NAME)

    set(PKG_CONFIG_EXECUTABLE pkg-config)
    set(GSETTINGS_DIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/glib-2.0/schemas")

    # Run the validator and error if it fails
    execute_process (COMMAND ${PKG_CONFIG_EXECUTABLE} gio-2.0 --variable glib_compile_schemas  OUTPUT_VARIABLE _glib_compile_schemas OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process (COMMAND ${_glib_compile_schemas} --dry-run --schema-file=${SCHEMA_NAME} ERROR_VARIABLE _schemas_invalid OUTPUT_STRIP_TRAILING_WHITESPACE)

    if (_schemas_invalid)
      message (SEND_ERROR "Schema validation error: ${_schemas_invalid}")
    endif (_schemas_invalid)

    # Actually install and recomple schemas
    message (STATUS "${GSETTINGS_DIR} is the GSettings install dir")
    install (FILES ${SCHEMA_NAME} DESTINATION ${GSETTINGS_DIR} OPTIONAL)

    install (CODE "message (STATUS \"Compiling GSettings schemas\")")
    install (CODE "execute_process (COMMAND ${_glib_compile_schemas} ${GSETTINGS_DIR})")
endmacro()

