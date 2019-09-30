
AC_INIT(indicator-messages, 12.10.5)

AC_PREREQ(2.62)

AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([check-news])

AM_MAINTAINER_MODE

IT_PROG_INTLTOOL([0.35.0])

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_C_O
AC_STDC_HEADERS
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_SUBST(VERSION)
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_ARG_ENABLE([deprecations],
  [AS_HELP_STRING([--enable-deprecations],
    [allow deprecated API usage @<:@default=yes@:>@])],
  [],
  [enable_deprecations=yes])
AS_IF([test "x$enable_deprecations" = xno],
  [CFLAGS="$CFLAGS -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE -DGTK_DISABLE_SINGLE_INCLUDES"]
)

# the Google Test targets are cpp
AC_PROG_CXX

###########################
# Dependencies 
###########################

GIO_UNIX_REQUIRED_VERSION=2.33.10
GLIB_REQUIRED_VERSION=2.35.4
INTROSPECTION_REQUIRED_VERSION=1.32.0

PKG_CHECK_MODULES(APPLET, gio-unix-2.0 >= $GIO_UNIX_REQUIRED_VERSION
                          glib-2.0 >= $GLIB_REQUIRED_VERSION)

PKG_CHECK_MODULES(GIO, gio-unix-2.0 >= $GIO_UNIX_REQUIRED_VERSION)

AC_SUBST(APPLET_CFLAGS)
AC_SUBST(APPLET_LIBS)

GLIB_GSETTINGS

GTK_DOC_CHECK([1.18], [--flavour no-tmpl])

GOBJECT_INTROSPECTION_CHECK([$INTROSPECTION_REQUIRED_VERSION])

AC_ARG_WITH([indicator-dir],
	    [AS_HELP_STRING([--with-indicator-dir=DIR], [Indicator directory [default=$datadir/unity/indicators]])],
	    [],
	    [with_indicator_dir=$datadir/unity/indicators])
AC_SUBST([INDICATOR_DIR], [$with_indicator_dir])

###########################
# gcov coverage reporting
###########################

m4_include([m4/gcov.m4])
AC_TDD_GCOV
AM_CONDITIONAL([HAVE_GCOV], [test "x$ac_cv_check_gcov" = xyes])
AM_CONDITIONAL([HAVE_LCOV], [test "x$ac_cv_check_lcov" = xyes])
AM_CONDITIONAL([HAVE_GCOVR], [test "x$ac_cv_check_gcovr" = xyes])
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_CXXFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

###########################
# Tests
###########################

AC_ARG_ENABLE([tests],
    [AS_HELP_STRING([--disable-tests], [Disable test scripts and tools (default=auto)])],
    [enable_tests=${enableval}],
    [enable_tests=auto])
if test "x$enable_tests" != "xno"; then
    m4_include([m4/gtest.m4])
    CHECK_GTEST
    AM_PATH_PYTHON(3.0,, [:])
    AC_PYTHON_MODULE(dbusmock)
    if test "x$have_gtest" = "xyes" -a "x$HAVE_PYMOD_DBUSMOCK" = "xyes"; then
        enable_tests="yes"
    else
        if test "x$enable_tests" = "xyes"; then
            AC_MSG_ERROR([tests were requested but gtest or dbusmock are not installed.])
        else
            enable_tests="no"
        fi
    fi
fi
AM_CONDITIONAL([BUILD_TESTS],[test "x$enable_tests" = "xyes"])

###########################
# Vala API Generation
###########################

AC_ARG_ENABLE([vala],
        AC_HELP_STRING([--disable-vala], [Disable vala]),
        [enable_vala=$enableval], [enable_vala=yes])

AS_IF([test "x$enable_vala" != "xno"],[
        AM_COND_IF([HAVE_INTROSPECTION],,[
                AC_MSG_ERROR([Vala bindings require introspection support, please --enable-introspection])
        ])
AC_PATH_PROG([VALA_API_GEN], [vapigen])
])
AM_CONDITIONAL([HAVE_VALA], [test -n "$VALA_API_GEN"])

##############################
# Custom Junk
##############################

AC_DEFUN([AC_DEFINE_PATH], [
	test "x$prefix" = xNONE && prefix="$ac_default_prefix"
	test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
	ac_define_path=`eval echo [$]$2`
	ac_define_path=`eval echo [$]ac_define_path`
	$1="$ac_define_path"
	AC_SUBST($1)
	ifelse($3, ,
		AC_DEFINE_UNQUOTED($1, "$ac_define_path"),
		AC_DEFINE_UNQUOTED($1, "$ac_define_path", $3))
])

###########################
# Internationalization
###########################

GETTEXT_PACKAGE=indicator-messages
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the default get text domain])
AC_DEFINE_PATH(GNOMELOCALEDIR, "${datadir}/locale", [locale directory])

AM_GLIB_GNU_GETTEXT

###########################
# Files
###########################

AC_OUTPUT([
Makefile
src/Makefile
common/Makefile
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/status/Makefile
data/icons/16x16/categories/Makefile
data/icons/22x22/Makefile
data/icons/22x22/status/Makefile
data/icons/22x22/categories/Makefile
data/icons/24x24/Makefile
data/icons/24x24/status/Makefile
data/icons/32x32/Makefile
data/icons/32x32/status/Makefile
data/icons/32x32/categories/Makefile
data/icons/48x48/Makefile
data/icons/48x48/status/Makefile
data/icons/scalable/Makefile
data/icons/scalable/status/Makefile
data/icons/scalable/categories/Makefile
data/upstart/Makefile
po/Makefile.in
test/Makefile
libmessaging-menu/Makefile
libmessaging-menu/messaging-menu.pc
doc/Makefile
doc/reference/Makefile
doc/reference/messaging-menu-docs.xml
])

###########################
# Results
###########################

AC_MSG_NOTICE([

Messaging Indicator Configuration:

	Prefix:        $prefix
	Indicator Dir: $INDICATORDIR
	tests:         $enable_tests
	gcov:          $use_gcov
	introspecion:  $enable_introspection
	Vala bindings: $enable_vala
	documentation: $enable_gtk_doc
])
