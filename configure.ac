AC_PREREQ([2.63])
AC_INIT([nm-applet],
        [1.0.10],
        [https://bugzilla.gnome.org/enter_bug.cgi?product=NetworkManager],
        [network-manager-applet])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 subdir-objects no-dist-gzip dist-bzip2 -Wno-portability])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

dnl
dnl Require programs
dnl
AC_PROG_CC
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG

dnl Define _GNU_SOURCE for various things like strcasestr()
AC_USE_SYSTEM_EXTENSIONS

dnl Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT

m4_define([nma_git_sha], [m4_esyscmd([ ( [ -d ./.git/ ] && [ "$(readlink -f ./.git/)" = "$(readlink -f "$(git rev-parse --git-dir 2>/dev/null)" 2>/dev/null)" ] && git rev-parse --verify -q HEAD 2>/dev/null ) || true ])])
NMA_GIT_SHA=nma_git_sha
AC_DEFINE_UNQUOTED(NMA_GIT_SHA,"$NMA_GIT_SHA",[git commit id of the original source code version])

dnl
dnl Checks for typedefs, structures, and compiler characteristics.
dnl
AC_TYPE_PID_T

dnl
dnl translation support
dnl
IT_PROG_INTLTOOL([0.50.1])

GETTEXT_PACKAGE=nm-applet
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package])
AM_GLIB_GNU_GETTEXT

# Check for iso-codes for country names translation
AC_MSG_CHECKING([whether to disable iso-codes at build-time])
AC_ARG_ENABLE([iso-codes],
    AS_HELP_STRING([--disable-iso-codes],[do not check for iso-codes at build-time]),
    [],[disable_iso_codes_check=no])
if test x$disable_iso_codes_check = xno ; then
    AC_MSG_RESULT([no])
else
    AC_MSG_RESULT([yes])
fi

if test x$disable_iso_codes_check = "xno" ; then
    AC_MSG_CHECKING([whether iso-codes has iso_3166 domain])
    if $PKG_CONFIG --variable=domains iso-codes | grep iso_3166 >/dev/null ; then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
    fi
    AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
    PKG_CHECK_MODULES(ISO_CODES, [iso-codes],
        [],
        [echo -e "\n$ISO_CODES_PKG_ERRORS.\n"
         echo "Consider installing the package or adjusting the PKG_CONFIG_PATH environment variable."
         echo "You can also disable build-time check for 'iso-codes' via --disable-iso-codes";
         exit 1;])
else
    AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["$prefix"],[ISO codes prefix])
fi

PKG_CHECK_MODULES(GOBJECT, gobject-2.0)

PKG_CHECK_MODULES(NMA,
		[dbus-glib-1 >= 0.74
		 gio-2.0 >= 2.32
		 NetworkManager >= 1.0.0
		 libnm-glib >= 1.0.0
		 libnm-util >= 1.0.0
		 libnm-glib-vpn >= 1.0.0
		 gmodule-export-2.0])

NMA_CFLAGS="$NMA_CFLAGS -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32"

AC_MSG_CHECKING([whether to build nm-applet-migration-tool])
AC_ARG_ENABLE([migration],
	      [AS_HELP_STRING([--disable-migration], [Don't build migration tool for NM <= 0.8 settings])],
	      [enable_migration="$enableval"],
	      [if test "$UNDER_JHBUILD" = "true"; then
	           enable_migration=no
	       else
	           enable_migration=yes
	       fi])
AC_MSG_RESULT([$enable_migration])
if test "$enable_migration" = "yes"; then
	PKG_CHECK_MODULES(MIGRATION, [gconf-2.0 gnome-keyring-1], :,
			  [AC_MSG_ERROR([
Could not find devel files needed for nm-applet-migration-tool:

$MIGRATION_PKG_ERRORS

You can configure with --disable-migration if you want to build
without the code to migrate GConf settings from older (< 0.9)
nm-applet releases.])])

	AC_SUBST(MIGRATION_CFLAGS)
	AC_SUBST(MIGRATION_LIBS)

	AC_DEFINE(BUILD_MIGRATION_TOOL, 1, [Define when building nm-applet-migration-tool])
fi
AM_CONDITIONAL(BUILD_MIGRATION_TOOL, test "$enable_migration" = "yes")

PKG_CHECK_MODULES(LIBSECRET, [libsecret-unstable])
AC_SUBST(LIBSECRET_CFLAGS)
AC_SUBST(LIBSECRET_LIBS)

# Check for libnotify >= 0.7
PKG_CHECK_MODULES(LIBNOTIFY_07, [libnotify >= 0.7], [have_libnotify_07=yes],[have_libnotify_07=no])
if test x"$have_libnotify_07" = "xyes"; then
	AC_DEFINE(HAVE_LIBNOTIFY_07, 1, [Define if you have libnotify 0.7 or later])
fi
PKG_CHECK_MODULES(NOTIFY, [libnotify >= 0.4.3])
AC_SUBST(NOTIFY_CFLAGS)
AC_SUBST(NOTIFY_LIBS)

PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 3.4)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)
GTK_CFLAGS="$GTK_CFLAGS -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_4"

dnl Check for gnome-bluetooth
AC_ARG_WITH([bluetooth],
	    AS_HELP_STRING([--with-bluetooth|--without-bluetooth], [Enable Bluetooth support]),
	    with_bluetooth="$withval",with_bluetooth=yes)
have_gbt=no
case "${with_bluetooth}" in
	no)	AC_MSG_NOTICE(Bluetooth support disabled)
		;;
	*)
		PKG_CHECK_MODULES(GNOME_BLUETOOTH,
				  gnome-bluetooth-1.0 >= 2.27.6
				  gnome-bluetooth-1.0 < 3.11.0
				  libnm-util libnm-glib,
				  have_gbt=yes, have_gbt=no)
		;;
esac

AM_CONDITIONAL(HAVE_GBT, test x"$have_gbt" = "xyes")

dnl ModemManager1 with libmm-glib
AC_ARG_WITH(modem-manager-1, AS_HELP_STRING([--with-modem-manager-1], [Enable new ModemManager1 interface support]),,[with_modem_manager_1=auto])
if (test "${with_modem_manager_1}" != "no"); then
    PKG_CHECK_MODULES(MM_GLIB,
                      [mm-glib],
                      [have_libmm_glib=yes],
                      [have_libmm_glib=no])
    AC_SUBST(MM_GLIB_CFLAGS)
    AC_SUBST(MM_GLIB_LIBS)

    if (test "${have_libmm_glib}" = "no"); then
        if (test "${with_modem_manager_1}" = "yes"); then
            AC_MSG_ERROR([Couldn't find libmm-glib])
        fi
    else
        with_modem_manager_1="yes"
    fi
fi

if (test "${with_modem_manager_1}" = "yes"); then
    AC_DEFINE(WITH_MODEM_MANAGER_1, 1, [Define if you have ModemManager1 support])
else
	AC_DEFINE(WITH_MODEM_MANAGER_1, 0, [Define if you have ModemManager1 support])
fi
AM_CONDITIONAL(WITH_MODEM_MANAGER_1, test "${with_modem_manager_1}" = "yes")

dnl Check for gudev
PKG_CHECK_MODULES(GUDEV, gudev-1.0 >= 147)
AC_SUBST(GUDEV_CFLAGS)
AC_SUBST(GUDEV_LIBS)

dnl Check for gobject introspection
GOBJECT_INTROSPECTION_CHECK([0.9.6])

GLIB_CONFIG_NMA
GLIB_SETTINGS

dnl
dnl Compiler flags
dnl
NM_COMPILER_WARNINGS
# Use --enable-maintainer-mode to disabled deprecated symbols
GNOME_MAINTAINER_MODE_DEFINES


AC_CONFIG_FILES([
Makefile
src/Makefile
src/libnm-gtk/Makefile
src/libnm-gtk/tests/Makefile
src/libnm-gtk/examples/Makefile
src/libnm-gtk/libnm-gtk.pc
src/marshallers/Makefile
src/utils/Makefile
src/utils/tests/Makefile
src/gconf-helpers/Makefile
src/gconf-helpers/tests/Makefile
src/wireless-security/Makefile
src/connection-editor/Makefile
src/gnome-bluetooth/Makefile
icons/Makefile
icons/16/Makefile
icons/22/Makefile
icons/32/Makefile
icons/48/Makefile
icons/scalable/Makefile
po/Makefile.in
man/Makefile
man/nm-applet.1
man/nm-connection-editor.1
org.gnome.nm-applet.gschema.xml
])
AC_OUTPUT
