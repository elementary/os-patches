# Copyright (C) 2015 Colin Walters <walters@verbum.org>
# SPDX-License-Identifier: LGPL-2.0-or-later
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

EXTRA_DIST += \
	subprojects/libglnx/README.md \
	subprojects/libglnx/COPYING \
	subprojects/libglnx/LICENSES/LGPL-2.0-or-later.txt \
	subprojects/libglnx/LICENSES/LGPL-2.1-or-later.txt \
	subprojects/libglnx/libglnx.m4 \
	$(NULL)

BUILT_SOURCES += $(top_builddir)/libglnx-config.h
CLEANFILES += $(top_builddir)/libglnx-config.h
$(top_builddir)/libglnx-config.h: Makefile.am
	echo '#include "config.h"' > $@

libglnx_la_SOURCES = \
	subprojects/libglnx/glnx-macros.h \
	subprojects/libglnx/glnx-backport-autocleanups.h \
	subprojects/libglnx/glnx-backport-autoptr.h \
	subprojects/libglnx/glnx-backports.h \
	subprojects/libglnx/glnx-backports.c \
	subprojects/libglnx/glnx-local-alloc.h \
	subprojects/libglnx/glnx-local-alloc.c \
	subprojects/libglnx/glnx-errors.h \
	subprojects/libglnx/glnx-errors.c \
	subprojects/libglnx/glnx-console.h \
	subprojects/libglnx/glnx-console.c \
	subprojects/libglnx/glnx-dirfd.h \
	subprojects/libglnx/glnx-dirfd.c \
	subprojects/libglnx/glnx-fdio.h \
	subprojects/libglnx/glnx-fdio.c \
	subprojects/libglnx/glnx-lockfile.h \
	subprojects/libglnx/glnx-lockfile.c \
	subprojects/libglnx/glnx-missing-syscall.h \
	subprojects/libglnx/glnx-missing.h \
	subprojects/libglnx/glnx-xattrs.h \
	subprojects/libglnx/glnx-xattrs.c \
	subprojects/libglnx/glnx-shutil.h \
	subprojects/libglnx/glnx-shutil.c \
	subprojects/libglnx/libglnx.h \
	subprojects/libglnx/tests/libglnx-testlib.h \
	$(NULL)

libglnx_la_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags) -I$(builddir)
libglnx_la_LDFLAGS = -avoid-version -Bsymbolic-functions -export-symbols-regex "^glnx_" -no-undefined -export-dynamic 
libglnx_la_LIBADD = $(libglnx_libs)

libglnx_tests = test-libglnx-xattrs test-libglnx-fdio test-libglnx-errors test-libglnx-macros test-libglnx-shutil
TESTS += $(libglnx_tests)

libglnx_testlib_sources = subprojects/libglnx/tests/libglnx-testlib.c

check_PROGRAMS += $(libglnx_tests)
test_libglnx_xattrs_SOURCES = $(libglnx_testlib_sources) subprojects/libglnx/tests/test-libglnx-xattrs.c
test_libglnx_xattrs_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_xattrs_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_fdio_SOURCES = $(libglnx_testlib_sources) subprojects/libglnx/tests/test-libglnx-fdio.c
test_libglnx_fdio_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_fdio_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_errors_SOURCES = $(libglnx_testlib_sources) subprojects/libglnx/tests/test-libglnx-errors.c
test_libglnx_errors_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_errors_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_macros_SOURCES = $(libglnx_testlib_sources) subprojects/libglnx/tests/test-libglnx-macros.c
test_libglnx_macros_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_macros_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_shutil_SOURCES = $(libglnx_testlib_sources) subprojects/libglnx/tests/test-libglnx-shutil.c
test_libglnx_shutil_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_shutil_LDADD = $(libglnx_libs) libglnx.la
