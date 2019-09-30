#
# shamelessly stolen from clutter-gtk
#
m4_define([ido_major_version], [13])
m4_define([ido_minor_version], [10])
m4_define([ido_micro_version], [1])

m4_define([ido_api_version],
          [ido_major_version.ido_minor_version])
m4_define([ido_version],
          [ido_major_version.ido_minor_version.ido_micro_version])

m4_define([ido_interface_age], [0])
m4_define([ido_binary_age],
          [m4_eval(100 * ido_minor_version + ido_micro_version)])

AC_PREREQ([2.64])

AC_INIT([ido],
        [ido_version],
        [http://bugs.launchpad.net/ido],
        [ido],
        [http://launchpad.net/ido])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/libido.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([check-news 1.11 foreign])

AM_SILENT_RULES([yes])

IDO_MAJOR_VERSION=ido_major_version
IDO_MINOR_VERSION=ido_minor_version
IDO_MICRO_VERSION=ido_micro_version
IDO_VERSION=ido_version
AC_SUBST(IDO_MAJOR_VERSION)
AC_SUBST(IDO_MINOR_VERSION)
AC_SUBST(IDO_MICRO_VERSION)
AC_SUBST(IDO_VERSION)

m4_define([lt_current],
          [m4_eval(100 * ido_minor_version + ido_micro_version - ido_interface_age)])
m4_define([lt_revision], [ido_interface_age])
m4_define([lt_age], [m4_eval(ido_binary_age - ido_interface_age)])
IDO_LT_CURRENT=lt_current
IDO_LT_REV=lt_revision
IDO_LT_AGE=lt_age
IDO_LT_VERSION="$IDO_LT_CURRENT:$IDO_LT_REV:$IDO_LT_AGE"
IDO_LT_LDFLAGS="-version-info $IDO_LT_VERSION"

AC_SUBST(IDO_LT_VERSION)
AC_SUBST(IDO_LT_LDFLAGS)

dnl ===========================================================================

# Checks for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
PKG_PROG_PKG_CONFIG

# Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures and compiler charecteristics
AC_C_CONST

# Checks for library functions
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset munmap strcasecmp strdup])
AC_CHECK_LIBM

GIO_REQUIRED_VERSION=2.37.0
GTK_REQUIRED_VERSION=3.8.2

PKG_CHECK_MODULES(GTK,[gtk+-3.0 >= $GTK_REQUIRED_VERSION
                       gio-2.0 >= $GIO_REQUIRED_VERSION])

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

dnl ===========================================================================

if test "x$GCC" = "xyes"; then
  GCC_FLAGS="-g -Wall"
fi
AC_SUBST(GCC_FLAGS)

# use strict compiler flags only on development releases
m4_define([maintainer_flags_default], [m4_if(m4_eval(ido_minor_version % 2), [1], [yes], [no])])
AC_ARG_ENABLE([maintainer-flags],
              [AS_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],
                              [Use strict compiler flags @<:@default=no@:>@])],
              [],
              [enable_maintainer_flags=maintainer_flags_default])

MAINTAINER_CFLAGS=""
AS_IF([test "x$enable_maintainer_flags" = "xyes" && test "x$GCC" = "xyes"],
      [
        MAINTAINER_CFLAGS="-Werror -Wall -Wshadow -Wcast-align -Wno-uninitialized -Wempty-body -Wformat-security -Winit-self"
      ]
)

AC_SUBST(MAINTAINER_CFLAGS)

dnl = gcov Coverage Reporting =================================================

m4_include([m4/gcov.m4])
AC_TDD_GCOV
AM_CONDITIONAL([HAVE_GCOV], [test "x$ac_cv_check_gcov" = xyes])
AM_CONDITIONAL([HAVE_LCOV], [test "x$ac_cv_check_lcov" = xyes])
AM_CONDITIONAL([HAVE_GCOVR], [test "x$ac_cv_check_gcovr" = xyes])
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_CXXFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

dnl = GObject Introspection ===================================================

GOBJECT_INTROSPECTION_CHECK([0.6.7])

dnl = Vala API Generation =====================================================

AC_PATH_PROG([VALA_API_GEN], [vapigen])

dnl = Google Test Framework ===================================================

dnl xorg-gtest also provides gtest.
CHECK_XORG_GTEST

dnl = GTK Doc Check ===========================================================

GTK_DOC_CHECK([1.8])

dnl ===========================================================================

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  example/Makefile
  tests/Makefile
  libido.pc
  libido3.pc
])

AC_OUTPUT

echo ""
echo " ido $VERSION"
echo " ==============================="
echo ""
echo " Prefix       : ${prefix}"
echo " gcov         : ${use_gcov}"
echo " introspection: ${enable_introspection}"
echo ""
echo " Documentation: ${enable_gtk_doc}"
echo ""
