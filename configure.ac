dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT([libaccounts-glib],
        [1.15],
        [http://code.google.com/p/accounts-sso/issues/entry],
        [libaccounts-glib],
        [http://code.google.com/p/accounts-sso/])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([libaccounts-glib.pc.in])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wall check-news nostdinc silent-rules subdir-objects])

AC_PROG_CC
AC_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_SED

LT_PREREQ([2.2])
LT_INIT([disable-static])

# Gobject Introspection
GOBJECT_INTROSPECTION_CHECK([1.30.0])

LIBACCOUNTS_REQUIRED="glib-2.0 >= 2.26
     gio-2.0 >= 2.30
     gio-unix-2.0
     gobject-2.0 >= 2.35.1
     libxml-2.0
     sqlite3 >= 3.7.0"
PKG_CHECK_MODULES([LIBACCOUNTS], [$LIBACCOUNTS_REQUIRED])
AC_SUBST(LIBACCOUNTS_CFLAGS)
AC_SUBST(LIBACCOUNTS_LIBS)

# Check for clock_gettime, as in glibc 2.17 it is in libc but in older versions
# it was in librt.
AC_SEARCH_LIBS([clock_gettime], [rt])

# Build tests.

CHECK_REQUIRED="check >= 0.9.4"

AC_ARG_ENABLE([tests],
  [AS_HELP_STRING([--disable-tests], [build with testing support disabled])])

AS_IF([test "x$enable_tests" != "xno"],
  [PKG_CHECK_EXISTS([$CHECK_REQUIRED], [have_check=yes], [have_check=no])],
  [have_check=no])

AS_IF([test "x$have_check" = "xyes"],
  [PKG_CHECK_MODULES([CHECK], [$CHECK_REQUIRED $LIBACCOUNTS_REQUIRED])
   AC_SUBST([CHECK_CFLAGS])
   AC_SUBST([CHECK_LIBS])
   AC_MSG_CHECKING([where to install tests])
   AC_ARG_WITH([testdir],
               [AS_HELP_STRING([--with-testdir=DIR],
                               [installation directory for tests [default=PKGLIBDIR]])],
               [testdir="$withval"],
               [testdir="\${pkglibdir}"])
   AC_MSG_RESULT([$testdir])
   AC_SUBST([testdir])

   AC_MSG_CHECKING([where to install test data])
   AC_ARG_WITH([testdatadir],
               [AS_HELP_STRING([--with-testdatadir=DIR],
                               [installation directory for test data [default=PKGDATADIR/testdata]])],
               [testdatadir="$withval"],
               [testdatadir="\${pkgdatadir}/testdata"])
   AC_MSG_RESULT([$testdatadir])
   AC_SUBST([testdatadir])

   # gcov coverage reporting
   m4_include([m4/gcov.m4])
   AC_TDD_GCOV
   AC_SUBST(COVERAGE_CFLAGS)
   AC_SUBST(COVERAGE_CXXFLAGS)
   AC_SUBST(COVERAGE_LDFLAGS)
   ],
  [AS_IF([test "x$enable_tests" = "xyes"],
      [AC_MSG_ERROR([tests enabled but required dependencies were not found])])])

AM_CONDITIONAL([ENABLE_TESTS], [test "x$have_check" = "xyes"])

GTK_DOC_CHECK([1.14], [--flavour no-tmpl])

AC_SUBST([DATABASE_DIR], [libaccounts-glib])
AC_DEFINE_UNQUOTED([DATABASE_DIR], ["$DATABASE_DIR"],
		   [Directory for storing database (in $XDG_CONFIG_HOME)])

AC_SUBST([APPLICATION_FILES_DIR], [accounts/applications])
AC_DEFINE_UNQUOTED([APPLICATION_FILES_DIR], ["$APPLICATION_FILES_DIR"],
		   [Directory for storing application definition files])

AC_SUBST([SERVICE_TYPE_FILES_DIR], [accounts/service_types])
AC_DEFINE_UNQUOTED([SERVICE_TYPE_FILES_DIR], ["$SERVICE_TYPE_FILES_DIR"],
		   [Directory for storing service type files])

AC_SUBST([SERVICE_FILES_DIR], [accounts/services])
AC_DEFINE_UNQUOTED([SERVICE_FILES_DIR], ["$SERVICE_FILES_DIR"],
		   [Directory for storing service files])

AC_SUBST([PROVIDER_FILES_DIR], [accounts/providers])
AC_DEFINE_UNQUOTED([PROVIDER_FILES_DIR], ["$PROVIDER_FILES_DIR"],
		   [Directory for storing provider files])

AC_ARG_ENABLE([cast-checks],
    [AS_HELP_STRING([--disable-cast-checks], [compile with GLib cast checks disabled])])
AS_IF([test "x$enable_cast_checks" = "xno"],
    [CFLAGS="$CFLAGS -DG_DISABLE_CAST_CHECKS"])

AC_ARG_ENABLE([asserts], 
    [AS_HELP_STRING([--disable-asserts], [compile with GLib assertions disabled])])
AS_IF([test "x$enable_asserts" = "xno"],
    [CFLAGS="$CFLAGS -DG_DISABLE_ASSERTS"])

AC_ARG_ENABLE([checks],
    [AS_HELP_STRING([--disable-checks], [compile with GLib checks disabled])])
AS_IF([test "x$checks" = "xno"],
    [CFLAGS="$CFLAGS -DG_DISABLE_CHECKS"])

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug], [enable printing of debug messages])])
AS_IF([test "x$enable_debug" = "xyes"],
    [CFLAGS="$CFLAGS -DENABLE_DEBUG"])

# SQLite's WAL journaling mode
AC_ARG_ENABLE([wal],
    [AS_HELP_STRING([--disable-wal], [Disable WAL journal mode in SQLite])])
AS_IF([test "x$enable_wal" = "xno"],
    [CFLAGS="$CFLAGS -DDISABLE_WAL"])

# Python support

PYGOBJECT_REQUIRED=2.90

AC_ARG_ENABLE([python],
              [AS_HELP_STRING([--enable-python], [Build with python support])])

AS_IF([test "x$enable_python" != "xno"],
    [PKG_CHECK_EXISTS([pygobject-3.0 >= $PYGOBJECT_REQUIRED],
                      [have_python=yes], [have_python=no])])

AS_IF([test "x$have_python" = "xyes"],
    [AM_PATH_PYTHON
     AC_SUBST([pyoverridesdir], [`$PYTHON -c "import gi;print (gi._overridesdir)" 2>/dev/null`])])

AM_CONDITIONAL([ENABLE_PYTHON], [test "x$have_python" = "xyes"])

AC_PATH_PROG([XMLLINT], [xmllint], [notfound])
AS_IF([test "x$XMLLINT" = "xnotfound"],
    [have_xmllint=no],
    [have_xmllint=yes])

AS_IF([test "x$have_xmllint" = "xyes"],
    [AC_SUBST([XMLLINT])],
    [AC_MSG_WARN([testing of XML files against DTDs disabled, as xmllint was not found])])

AM_CONDITIONAL([HAVE_XMLLINT], [test "x$have_xmllint" = "xyes"])

# Man page, generated with gtk-doc and xsltproc from DocBook XML.
AC_ARG_ENABLE([man],
    [AS_HELP_STRING([--disable-man], [Disable building the man page])])

AS_IF([test "x$enable_man" != "xno"],
    [AC_CHECK_PROG([have_xsltproc], [xsltproc], [yes], [no])
     JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.3//EN],
         [DocBook XML DTD V4.3], [have_docbookdtd=yes], [have_docbookdtd=no])
     JH_CHECK_XML_CATALOG(
         [http://docbook.sourceforge.net/release/xsl/current/html/refentry.xsl],
         [DocBook XSL Stylesheets], [have_docbookxsl=yes],
         [have_docbookxsl=no])
     AS_IF([test "x$have_xsltproc" = "xyes" -a "x$have_docbookdtd" = "xyes" \
         -a "x$have_docbookxsl" = "xyes"],
         [have_manutils=yes],
         [AS_IF([test "x$enable_man" = "xyes"],
             [AC_MSG_ERROR(
                 [man page generation requested but required utilities were not found])])
          have_manutils=no])],
    [have_manutils=no])

AM_CONDITIONAL([ENABLE_MAN], [test "x$have_manutils" = "xyes"])

AC_CONFIG_FILES([
	Makefile
	libaccounts-glib/Makefile
	libaccounts-glib.pc
	docs/Makefile
	docs/reference/Makefile
	docs/reference/version.xml
	pygobject/Makefile
	tests/Makefile
	tools/Makefile
])
AC_OUTPUT
