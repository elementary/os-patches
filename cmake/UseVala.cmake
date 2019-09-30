# - Precompilation of Vala/Genie source files into C sources
# Makes use of the parallel build ability introduced in Vala 0.11. Derived
# from a similar module by Jakob Westhoff and the original GNU Make rules.
# Might be a bit oversimplified.
#
# This module defines three functions. The first one:
#
#   vala_init (id
#     [DIRECTORY dir]          - Output directory (binary dir by default)
#     [PACKAGES package...]    - Package dependencies
#     [OPTIONS option...]      - Extra valac options
#     [CUSTOM_VAPIS file...])  - Custom vapi files to include in the build
#     [DEPENDS targets...])    - Extra dependencies for code generation
#
# initializes a single precompilation unit using the given arguments.
# You can put files into it via the following function:
#
#   vala_add (id source.vala
#     [DEPENDS source...])     - Vala/Genie source or .vapi dependencies
#
# Finally retrieve paths for generated C files by calling:
#
#   vala_finish (id
#     [SOURCES sources_var]          - Input Vala/Genie sources
#     [OUTPUTS outputs_var]          - Output C sources
#     [GENERATE_HEADER id.h          - Generate id.h and id_internal.h
#       [GENERATE_VAPI id.vapi]      - Generate a vapi file
#       [GENERATE_SYMBOLS id.def]])  - Generate a list of public symbols
#

#=============================================================================
# Copyright Přemysl Janouch 2011
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
# OF SUCH DAMAGE.
#=============================================================================

find_package (Vala 0.20 REQUIRED)
include (CMakeParseArguments)

function (vala_init id)
	set (_multi_value PACKAGES OPTIONS CUSTOM_VAPIS DEPENDS)
	cmake_parse_arguments (arg "" "DIRECTORY" "${_multi_value}" ${ARGN})

	if (arg_DIRECTORY)
		set (directory ${arg_DIRECTORY})
		if (NOT IS_DIRECTORY ${directory})
			file (MAKE_DIRECTORY ${directory})
		endif (NOT IS_DIRECTORY ${directory})
	else (arg_DIRECTORY)
		set (directory ${CMAKE_CURRENT_BINARY_DIR})
	endif (arg_DIRECTORY)

	set (pkg_opts)
	foreach (pkg ${arg_PACKAGES})
		list (APPEND pkg_opts "--pkg=${pkg}")
	endforeach (pkg)

	set (VALA_${id}_DIR "${directory}" PARENT_SCOPE)
	set (VALA_${id}_ARGS ${pkg_opts} ${arg_OPTIONS}
		${arg_CUSTOM_VAPIS} PARENT_SCOPE)
	set (VALA_${id}_DEPENDS ${arg_DEPENDS}
		PARENT_SCOPE)

	set (VALA_${id}_SOURCES "" PARENT_SCOPE)
	set (VALA_${id}_OUTPUTS "" PARENT_SCOPE)
	set (VALA_${id}_FAST_VAPI_FILES "" PARENT_SCOPE)
	set (VALA_${id}_FAST_VAPI_ARGS "" PARENT_SCOPE)
endfunction (vala_init)

function (vala_add id file)
	cmake_parse_arguments (arg "" "" "DEPENDS" ${ARGN})

	if (NOT IS_ABSOLUTE "${file}")
		set (file "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
	endif (NOT IS_ABSOLUTE "${file}")

	get_filename_component (output_name "${file}" NAME)
	get_filename_component (output_base "${file}" NAME_WE)
	set (output_base "${VALA_${id}_DIR}/${output_base}")

	# XXX: It would be best to have it working without touching the vapi
	#      but it appears this cannot be done in CMake.
	add_custom_command (OUTPUT "${output_base}.vapi"
		COMMAND ${VALA_COMPILER} "${file}" "--fast-vapi=${output_base}.vapi"
		COMMAND ${CMAKE_COMMAND} -E touch "${output_base}.vapi"
		DEPENDS "${file}"
		COMMENT "Generating a fast vapi for ${output_name}" VERBATIM)

	set (vapi_opts)
	set (vapi_depends)
	foreach (vapi ${arg_DEPENDS})
		if (NOT IS_ABSOLUTE "${vapi}")
			set (vapi "${VALA_${id}_DIR}/${vapi}.vapi")
		endif (NOT IS_ABSOLUTE "${vapi}")

		list (APPEND vapi_opts "--use-fast-vapi=${vapi}")
		list (APPEND vapi_depends "${vapi}")
	endforeach (vapi)

	add_custom_command (OUTPUT "${output_base}.c"
		COMMAND ${VALA_COMPILER} "${file}" -C ${vapi_opts} ${VALA_${id}_ARGS}
		COMMAND ${CMAKE_COMMAND} -E touch "${output_base}.c"
		DEPENDS "${file}" ${vapi_depends} ${VALA_${id}_DEPENDS}
		WORKING_DIRECTORY "${VALA_${id}_DIR}"
		COMMENT "Precompiling ${output_name}" VERBATIM)

	set (VALA_${id}_SOURCES ${VALA_${id}_SOURCES}
		"${file}" PARENT_SCOPE)
	set (VALA_${id}_OUTPUTS ${VALA_${id}_OUTPUTS}
		"${output_base}.c" PARENT_SCOPE)
	set (VALA_${id}_FAST_VAPI_FILES ${VALA_${id}_FAST_VAPI_FILES}
		"${output_base}.vapi" PARENT_SCOPE)
	set (VALA_${id}_FAST_VAPI_ARGS ${VALA_${id}_FAST_VAPI_ARGS}
		"--use-fast-vapi=${output_base}.vapi" PARENT_SCOPE)
endfunction (vala_add)

function (vala_finish id)
	set (_one_value SOURCES OUTPUTS
		GENERATE_VAPI GENERATE_HEADER GENERATE_SYMBOLS)
	cmake_parse_arguments (arg "" "${_one_value}" "" ${ARGN})

	if (arg_SOURCES)
		set (${arg_SOURCES} ${VALA_${id}_SOURCES} PARENT_SCOPE)
	endif (arg_SOURCES)

	if (arg_OUTPUTS)
		set (${arg_OUTPUTS} ${VALA_${id}_OUTPUTS} PARENT_SCOPE)
	endif (arg_OUTPUTS)

	set (outputs)
	set (export_args)

	if (arg_GENERATE_VAPI)
		if (NOT IS_ABSOLUTE "${arg_GENERATE_VAPI}")
			set (arg_GENERATE_VAPI
				"${VALA_${id}_DIR}/${arg_GENERATE_VAPI}")
		endif (NOT IS_ABSOLUTE "${arg_GENERATE_VAPI}")

		list (APPEND outputs "${arg_GENERATE_VAPI}")
		list (APPEND export_args "--internal-vapi=${arg_GENERATE_VAPI}")

		if (NOT arg_GENERATE_HEADER)
			message (FATAL_ERROR "Header generation required for vapi")
		endif (NOT arg_GENERATE_HEADER)
	endif (arg_GENERATE_VAPI)

	if (arg_GENERATE_SYMBOLS)
		if (NOT IS_ABSOLUTE "${arg_GENERATE_SYMBOLS}")
			set (arg_GENERATE_SYMBOLS
				"${VALA_${id}_DIR}/${arg_GENERATE_SYMBOLS}")
		endif (NOT IS_ABSOLUTE "${arg_GENERATE_SYMBOLS}")

		list (APPEND outputs "${arg_GENERATE_SYMBOLS}")
		list (APPEND export_args "--symbols=${arg_GENERATE_SYMBOLS}")

		if (NOT arg_GENERATE_HEADER)
			message (FATAL_ERROR "Header generation required for symbols")
		endif (NOT arg_GENERATE_HEADER)
	endif (arg_GENERATE_SYMBOLS)

	if (arg_GENERATE_HEADER)
		if (NOT IS_ABSOLUTE "${arg_GENERATE_HEADER}")
			set (arg_GENERATE_HEADER
				"${VALA_${id}_DIR}/${arg_GENERATE_HEADER}")
		endif (NOT IS_ABSOLUTE "${arg_GENERATE_HEADER}")

		get_filename_component (header_path "${arg_GENERATE_HEADER}" PATH)
		get_filename_component (header_name "${arg_GENERATE_HEADER}" NAME_WE)
		set (header_base "${header_path}/${header_name}")
		get_filename_component (header_ext "${arg_GENERATE_HEADER}" EXT)

		list (APPEND outputs
			"${header_base}${header_ext}"
			"${header_base}_internal${header_ext}")
		list (APPEND export_args
			"--header=${header_base}${header_ext}"
			"--internal-header=${header_base}_internal${header_ext}")
	endif (arg_GENERATE_HEADER)

	if (outputs)
		add_custom_command (OUTPUT ${outputs}
			COMMAND ${VALA_COMPILER} -C ${VALA_${id}_ARGS}
				${export_args} ${VALA_${id}_FAST_VAPI_ARGS}
			DEPENDS ${VALA_${id}_FAST_VAPI_FILES}
			COMMENT "Generating vapi/headers/symbols" VERBATIM)
	endif (outputs)
endfunction (vala_finish id)


function (vapi_gen id)
	set (_one_value LIBRARY INPUT)
	set (_multi_value PACKAGES)
	cmake_parse_arguments (arg "" "${_one_value}" "${_multi_value}" ${ARGN})

	set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${id}.vapi")
	if (arg_LIBRARY)
		set (OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${arg_LIBRARY}.vapi")
	endif (arg_LIBRARY)
	
	set("${id}_OUTPUT" ${OUTPUT} PARENT_SCOPE)
	
	set(PACKAGE_LIST)
	foreach(PACKAGE ${arg_PACKAGES})
		list(APPEND PACKAGE_LIST "--pkg" ${PACKAGE})
	endforeach()
	
	add_custom_command(
		OUTPUT
			${OUTPUT}
		COMMAND
			${VAPI_GEN}
			${PACKAGE_LIST}
			--library=${arg_LIBRARY}
			${arg_INPUT}
		DEPENDS
			${arg_INPUT}
		VERBATIM
	)
	
	add_custom_target(${id} ALL DEPENDS ${OUTPUT})
endfunction (vapi_gen id)

