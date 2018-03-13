AC_PREREQ([2.63])

AC_INIT([geoclue],
        [2.4.7],
        [http://bugs.freedesktop.org/enter_bug.cgi?product=GeoClue],
        [geoclue],
        [http://www.freedesktop.org/wiki/Software/GeoClue])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar foreign -Wall -Werror -Wno-portability])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

AC_SUBST([GEOCLUE_API_VERSION], [2.0])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_AR
AC_CHECK_TOOL([DESKTOP_FILE_VALIDATE], [desktop-file-validate], [/bin/true])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Pkg-config
PKG_PROG_PKG_CONFIG([0.22])

# i18 support
IT_PROG_INTLTOOL(0.40.0)

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[The name of the gettext domain])

GLIB_MIN_VERSION=2.34.0
JSON_GLIB_MIN_VERSION=0.14
LIBMM_GLIB_MIN_VERSION=1.0
LIBSOUP_MIN_VERSION=2.42
AVAHI_MIN_VERSION=0.6.10

AC_CHECK_LIB([m],[sin])

dnl
dnl Documentation
dnl
GTK_DOC_CHECK(1.0)

PKG_CHECK_MODULES(GEOCLUE, [
  glib-2.0 >= $GLIB_MIN_VERSION
  gio-2.0 >= $GLIB_MIN_VERSION
  gio-unix-2.0 >= $GLIB_MIN_VERSION
  json-glib-1.0 >= $JSON_GLIB_MIN_VERSION
  libsoup-2.4 >= $LIBSOUP_MIN_VERSION
])

# GObject-introspection support
GOBJECT_INTROSPECTION_CHECK([0.9.6])
build_introspection=no

# libgeoclue
AC_ARG_ENABLE(libgeoclue,
              AS_HELP_STRING([--disable-libgeoclue], [Disable convenience library]),
              [build_libgeoclue=$enableval], [build_libgeoclue=yes])
if test "x$build_libgeoclue" = "xyes"; then
  PKG_CHECK_MODULES(LIBGEOCLUE, [
    glib-2.0 >= $GLIB_MIN_VERSION
    gio-2.0 >= $GLIB_MIN_VERSION
    gio-unix-2.0 >= $GLIB_MIN_VERSION
  ])
  if test "x$found_introspection" == "xyes"; then
     build_introspection=yes
  fi
  build_libgeoclue=yes

  AC_DEFINE([BUILD_LIBGEOCLUE], [1], [Build convenience library?])
else
  AC_DEFINE([BUILD_LIBGEOCLUE], [0], [Build convenience library?])
fi
AM_CONDITIONAL([BUILD_LIBGEOCLUE], [test "x$build_libgeoclue" = "xyes"])

# 3G source
AC_ARG_ENABLE(3g-source,
              AS_HELP_STRING([--disable-3g-source], [Disable 3G backend (requires ModemManager)]),
              [build_3g_source=$enableval], [build_3g_source=yes])
if test "x$build_3g_source" = "xyes"; then
  build_modem_source=yes
  AC_DEFINE([GCLUE_USE_3G_SOURCE], [1], [Build 3G source?])
else
  AC_DEFINE([GCLUE_USE_3G_SOURCE], [0], [Build 3G source?])
fi
AM_CONDITIONAL([BUILD_3G_SOURCE], [test "x$build_3g_source" = "xyes"])

# CDMA source
AC_ARG_ENABLE(cdma-source,
              AS_HELP_STRING([--disable-cdma-source], [Disable CDMA backend (requires ModemManager)]),
              [build_cdma_source=$enableval], [build_cdma_source=yes])
if test "x$build_cdma_source" = "xyes"; then
  build_modem_source=yes
  AC_DEFINE([GCLUE_USE_CDMA_SOURCE], [1], [Build CDMA source?])
else
  AC_DEFINE([GCLUE_USE_CDMA_SOURCE], [0], [Build CDMA source?])
fi
AM_CONDITIONAL([BUILD_CDMA_SOURCE], [test "x$build_cdma_source" = "xyes"])

# GPS source
AC_ARG_ENABLE(modem-gps-source,
              AS_HELP_STRING([--disable-modem-gps-source], [Disable modem GPS backend (requires ModemManager)]),
              [build_modem_gps_source=$enableval], [build_modem_gps_source=yes])
if test "x$build_modem_gps_source" = "xyes"; then
  build_modem_source=yes
  AC_DEFINE([GCLUE_USE_MODEM_GPS_SOURCE], [1], [Build modem GPS source?])
else
  AC_DEFINE([GCLUE_USE_MODEM_GPS_SOURCE], [0], [Build modem GPS source?])
fi
AM_CONDITIONAL([BUILD_MODEM_GPS_SOURCE], [test "x$build_modem_gps_source" = "xyes"])

# Modem source is used in common by GPS, 3G and CDMA sources
if test "x$build_modem_source" = "xyes"; then
  require_modemmanager=yes
fi
AM_CONDITIONAL([BUILD_MODEM_SOURCE], [test "x$build_modem_source" = "xyes"])

# Network NMEA Source
AC_ARG_ENABLE(nmea-source,
              AS_HELP_STRING([--disable-nmea-source],
                             [Disable network NMEA source (requires avahi-client and avahi-glib)]),
              [build_nmea_source=$enableval], [build_nmea_source=yes])

if test "x$build_nmea_source" = "xyes"; then
  AC_DEFINE([GCLUE_USE_NMEA_SOURCE], [1], [Build network NMEA source?])
else
  AC_DEFINE([GCLUE_USE_NMEA_SOURCE], [0], [Build network NMEA source?])
fi
AM_CONDITIONAL([BUILD_NMEA_SOURCE], [test "x$build_nmea_source" = "xyes"])

# Backend build
AC_ARG_ENABLE(backend,
              AS_HELP_STRING([--disable-backend], [Disable backend]),
              [build_backend=$enableval], [build_backend=yes])

if test "x$build_backend" = "xyes"; then
  # Check for ModemManager if one of the sources requires it
  if test "x$require_modemmanager" = "xyes"; then
    PKG_CHECK_MODULES(ModemManager, mm-glib >= $LIBMM_GLIB_MIN_VERSION)
  fi

  if test "x$build_nmea_source" = "xyes"; then
      # Check for avahi-client and avahi-glib
      PKG_CHECK_MODULES(NMEA_SOURCE, [
                        avahi-client >= $AVAHI_MIN_VERSION
                        avahi-glib >= $AVAHI_MIN_VERSION])
  fi
  AC_DEFINE([BUILD_BACKEND], [1], [Build backend?])
else
  AC_DEFINE([BUILD_BACKEND], [0], [Build backend?])
fi
AM_CONDITIONAL([BUILD_BACKEND], [test "x$build_backend" = "xyes"])

# Demo agent
AC_ARG_ENABLE(demo-agent,
              AS_HELP_STRING([--enable-demo-agent=yes|no],
                             [Build demo agent (default=no)]),
              [enable_demo_agent=$enableval],
              [enable_demo_agent=no])
if test "x$enable_demo_agent" = "xyes"; then
    PKG_CHECK_MODULES(DEMO_AGENT, [
                      glib-2.0 >= $GLIB_MIN_VERSION
                      gio-2.0 >= $GLIB_MIN_VERSION
                      gio-unix-2.0 >= $GLIB_MIN_VERSION
                      libnotify])
else
    AC_DEFINE([BUILD_DEMO_AGENT], [0], [Build demo agent?])
fi
AM_CONDITIONAL([BUILD_DEMO_AGENT], [test "x$enable_demo_agent" = "xyes"])

GNOME_COMPILE_WARNINGS([maximum])

dnl Debugging
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug],[enable debugging]),,
        enable_debug=no)
if test "x$enable_debug" = "xyes"; then
        CFLAGS="$CFLAGS -O0 -ggdb3"
fi

# DBus system directory
AC_ARG_WITH(dbus-sys-dir, AS_HELP_STRING([--with-dbus-sys-dir=DIR], [where D-BUS system.d directory is]))
if test -n "$with_dbus_sys_dir" ; then
    DBUS_SYS_DIR="$with_dbus_sys_dir"
else
    DBUS_SYS_DIR="${sysconfdir}/dbus-1/system.d"
fi
AC_SUBST(DBUS_SYS_DIR)

AC_ARG_WITH(dbus-service-user,
	    AS_HELP_STRING([--with-dbus-service-user=USER],
                           [The USER (existing) as which the service will run (default: root)]),
	    dbus_srv_user="$with_dbus_service_user",
	    dbus_srv_user="root")
AC_SUBST(dbus_srv_user)

# gdbus-codegen
if test x$cross_compiling != xyes; then
	GDBUS_CODEGEN=`$PKG_CONFIG --variable gdbus_codegen gio-2.0`
else
	AC_PATH_PROG([GDBUS_CODEGEN],[gdbus-codegen])
fi

AS_IF([test "$GDBUS_CODEGEN" = ""],[
	AC_MSG_ERROR([gdbus-codegen not found])
])

# systemd
AC_ARG_WITH([systemdsystemunitdir],
            AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
            [],
            [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != "xno"; then
  AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$systemdsystemunitdir"])

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

AC_CONFIG_FILES([
  Makefile
  build-aux/Makefile
  src/Makefile
  src/geoclue-$GEOCLUE_API_VERSION.pc:src/geoclue.pc.in
  src/agent/Makefile
  src/geocode-glib/Makefile
  public-api/Makefile
  po/Makefile.in
  data/org.freedesktop.GeoClue2.conf
  data/org.freedesktop.GeoClue2.Agent.conf
  data/Makefile
  libgeoclue/Makefile
  libgeoclue/libgeoclue-$GEOCLUE_API_VERSION.pc:libgeoclue/libgeoclue.pc.in
  demo/Makefile
  docs/Makefile
  docs/lib/Makefile
  docs/version.xml
  docs/lib/version.xml
])

AC_OUTPUT

dnl ==========================================================================
AC_MSG_NOTICE([

        GeoClue $VERSION
        =================

        prefix:                   ${prefix}
        c compiler:               ${CC} ${CFLAGS}
        systemdsystemunitdir:     ${systemdsystemunitdir}

        Backend:                  ${build_backend}
        Convenience library:      ${build_libgeoclue}
        Introspection:            ${build_introspection}
        3G source:                ${build_3g_source}
        CDMA source:              ${build_cdma_source}
        Modem GPS source:         ${build_modem_gps_source}
        Network NMEA source:      ${build_nmea_source}
])
