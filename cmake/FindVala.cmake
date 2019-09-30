# - Find Vala
# This module looks for valac.
# This module defines the following values:
#    VALA_FOUND
#    VALA_COMPILER
#    VALA_VERSION
#    VAPI_GEN
#    VAPI_GEN_VERSION

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

find_program (VALA_COMPILER "valac")

if (VALA_COMPILER)
	execute_process (COMMAND ${VALA_COMPILER} --version
		OUTPUT_VARIABLE VALA_VERSION)
	string (REGEX MATCH "[.0-9]+" VALA_VERSION "${VALA_VERSION}")
endif (VALA_COMPILER)

find_program (VAPI_GEN "vapigen")

if (VAPI_GEN)
	execute_process (COMMAND ${VAPI_GEN} --version
		OUTPUT_VARIABLE VAPI_GEN_VERSION)
	string (REGEX MATCH "[.0-9]+" VAPI_GEN_VERSION "${VAPI_GEN_VERSION}")
endif (VAPI_GEN)

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (Vala
	REQUIRED_VARS VALA_COMPILER
	VERSION_VAR VALA_VERSION)

mark_as_advanced (VALA_COMPILER VALA_VERSION VAPI_GEN)

