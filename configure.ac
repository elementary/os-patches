AC_INIT([indicator-bluetooth],[13.10.0])
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz foreign])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE

AM_PROG_VALAC([0.16.0])
AM_PROG_CC_C_O
LT_INIT

GLIB_GSETTINGS

dnl ###########################################################################
dnl Dependencies
dnl ###########################################################################

GLIB_REQUIRED_VERSION=2.36
GIO_REQUIRED_VERSION=2.36
URL_DISPATCHER_1_REQUIRED_VERSION=1

PKG_CHECK_MODULES(SERVICE_DEPS, [glib-2.0 >= $GLIB_REQUIRED_VERSION
                                 gio-unix-2.0 >= $GIO_REQUIRED_VERSION
                                 url-dispatcher-1 >= $URL_DISPATCHER_1_REQUIRED_VERSION])


dnl ##############################
dnl # Custom Junk
dnl ##############################

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

dnl ###########################
dnl # Internationalization
dnl ###########################

IT_PROG_INTLTOOL([0.41.0])

GETTEXT_PACKAGE=indicator-bluetooth
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the default get text domain])
AC_DEFINE_PATH(GNOMELOCALEDIR, "${datadir}/locale", [locale directory])

AM_GLIB_GNU_GETTEXT

dnl ###########################################################################
dnl Files to generate
dnl ###########################################################################

AC_OUTPUT([
Makefile
data/Makefile
po/Makefile.in
src/Makefile
])
