# Copyright (C) 2015 Colin Walters <walters@verbum.org>
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
	libglnx/README.md \
	libglnx/COPYING \
	libglnx/libglnx.m4 \
	$(NULL)

libglnx_la_SOURCES = \
	libglnx/glnx-macros.h \
	libglnx/glnx-backport-autocleanups.h \
	libglnx/glnx-backport-autoptr.h \
	libglnx/glnx-backports.h \
	libglnx/glnx-backports.c \
	libglnx/glnx-local-alloc.h \
	libglnx/glnx-local-alloc.c \
	libglnx/glnx-errors.h \
	libglnx/glnx-errors.c \
	libglnx/glnx-console.h \
	libglnx/glnx-console.c \
	libglnx/glnx-dirfd.h \
	libglnx/glnx-dirfd.c \
	libglnx/glnx-fdio.h \
	libglnx/glnx-fdio.c \
	libglnx/glnx-lockfile.h \
	libglnx/glnx-lockfile.c \
	libglnx/glnx-missing-syscall.h \
	libglnx/glnx-missing.h \
	libglnx/glnx-xattrs.h \
	libglnx/glnx-xattrs.c \
	libglnx/glnx-shutil.h \
	libglnx/glnx-shutil.c \
	libglnx/libglnx.h \
	libglnx/tests/libglnx-testlib.h \
	$(NULL)

libglnx_la_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
libglnx_la_LDFLAGS = -avoid-version -Bsymbolic-functions -export-symbols-regex "^glnx_" -no-undefined -export-dynamic 
libglnx_la_LIBADD = $(libglnx_libs)

libglnx_tests = test-libglnx-xattrs test-libglnx-fdio test-libglnx-errors test-libglnx-macros test-libglnx-shutil
TESTS += $(libglnx_tests)

check_PROGRAMS += $(libglnx_tests)
test_libglnx_xattrs_SOURCES = libglnx/tests/test-libglnx-xattrs.c
test_libglnx_xattrs_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_xattrs_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_fdio_SOURCES = libglnx/tests/test-libglnx-fdio.c
test_libglnx_fdio_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_fdio_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_errors_SOURCES = libglnx/tests/test-libglnx-errors.c
test_libglnx_errors_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_errors_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_macros_SOURCES = libglnx/tests/test-libglnx-macros.c
test_libglnx_macros_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_macros_LDADD = $(libglnx_libs) libglnx.la

test_libglnx_shutil_SOURCES = libglnx/tests/test-libglnx-shutil.c
test_libglnx_shutil_CFLAGS = $(AM_CFLAGS) $(libglnx_cflags)
test_libglnx_shutil_LDADD = $(libglnx_libs) libglnx.la
