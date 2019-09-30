#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(bamf, 0.5.1, dx-team@canonical.com)
AC_PREREQ(2.69)
AC_SUBST(LIBBAMF_VER, 3)

AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE([foreign tar-pax])
AM_SILENT_RULES([yes])

AC_CONFIG_MACRO_DIR([m4])

GNOME_COMMON_INIT
GNOME_COMPILE_WARNINGS(maximum)

AM_MAINTAINER_MODE
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
AC_ISC_POSIX
AC_SUBST(ACLOCAL_AMFLAFS, "$ACLOCAL_FLAGS -I m4")

AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test "x$PKG_CONFIG" = "xno"; then AC_MSG_ERROR([You need to install pkg-config]); fi

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_HEADER_STDC

SHAMROCK_EXPAND_LIBDIR
SHAMROCK_EXPAND_BINDIR
SHAMROCK_EXPAND_DATADIR

#
# pkg-config
#
PKG_PROG_PKG_CONFIG

#
# glib
#
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.32.0 gio-2.0 >= 2.30.0 gio-unix-2.0)

#
# gdbus-codegen
#
AC_PATH_PROG([GDBUS_CODEGEN], [gdbus-codegen])
if test -z "$GDBUS_CODEGEN"; then AC_MSG_ERROR([gdbus-codegen is required but was not found]); fi

#
# libwnck
#
PKG_CHECK_MODULES(WNCK, libwnck-3.0 >= 3.4.7)

#
# libgtop
#
PKG_CHECK_MODULES(GTOP, libgtop-2.0)

#
# x11
#
PKG_CHECK_MODULES(X, x11)

###########################
# GObject Introspection
###########################

GOBJECT_INTROSPECTION_CHECK([0.10.2])

###########################
# Vala API Generation
###########################

if test "$enable_introspection" = "yes"; then
  AC_PATH_PROG([VALA_API_GEN], [vapigen])
  if test -z "$VALA_API_GEN"; then
    AC_MSG_ERROR([vapigen is needed to build introspection])
    HAVE_INTROSPECTION=0
    enable_introspection="no"
  fi
fi

###########################
# GTK-DOC
###########################
GTK_DOC_CHECK(1.0)

###########################
# gtester2xunit checks    #
###########################

AC_PATH_PROG([PYTHON],[python])
AC_MSG_CHECKING(for gtester2xunit dependencies)
if !($PYTHON -c "import libxslt, libxml2" 2> /dev/null); then
  AC_MSG_RESULT([no])
  AC_MSG_ERROR([You need to install python-libxslt1 and python-libxml2]);
fi
AC_MSG_RESULT([yes])

###########################
# Headless tests
###########################
AC_ARG_ENABLE([headless-tests],
              AS_HELP_STRING([--enable-headless-tests=@<:@no/yes@:>@],[enable headless test suite (requires Xvfb) @<:@default=no@:>@]),,
              [enable_headless_tests=no])

AM_CONDITIONAL([ENABLE_HEADLESS_TESTS],[test "x$enable_headless_tests" != "xno"])

if test "x$enable_headless_tests" = "xyes"; then
  AC_PATH_PROG([XVFB],[Xvfb])
  AC_PATH_PROG([DBUS_LAUNCH],[dbus-launch])

  if test -z "$XVFB" -o -z "$DBUS_LAUNCH"; then
    AC_MSG_ERROR([Xvfb and dbus-launch are needed for headless-tests])
  fi
fi

###########################
# gcov coverage reporting
###########################
AC_TDD_GCOV
AM_CONDITIONAL([HAVE_GCOV], [test "x$ac_cv_check_gcov" = xyes])
AM_CONDITIONAL([HAVE_LCOV], [test "x$ac_cv_check_lcov" = xyes])
AM_CONDITIONAL([HAVE_GCOVR], [test "x$ac_cv_check_gcovr" = xyes])
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

#
# Gnome/GTK checks
#
AM_PATH_GTK_3_0

dnl CFLAGS
CFLAGS="$CFLAGS -Wall -Werror -Wempty-body -Wformat-security -Winit-self -Warray-bounds -Wimplicit-function-declaration -lm"

AC_SUBST(BAMFDAEMON_CFLAGS)
AC_SUBST(BAMFDAEMON_LIBS)

origgiomodulesdir=`pkg-config --variable=giomoduledir gio-2.0`
# Make giomodulesdir honour 'prefix', so that distcheck works.
giomodulesdir=`echo "$origgiomodulesdir" | sed 's|/usr|${prefix}|'`
AC_SUBST(giomodulesdir)

AC_OUTPUT([
Makefile
lib/libbamf-private/Makefile
lib/libbamf/Makefile
lib/libbamf/libbamf3.pc
src/Makefile
tests/Makefile
tests/bamfdaemon/Makefile
tests/libbamf/Makefile
data/Makefile
doc/Makefile
doc/reference/Makefile
doc/reference/libbamf/Makefile
])

cat <<EOF

${PACKAGE}-${VERSION}

  Build Environment
    Install Prefix:     ${prefix}
    Introspection:      ${enable_introspection}
    Headless tests:     ${enable_headless_tests}
    Coverage Reporting: ${use_gcov}

EOF
