AC_PREREQ([2.63])
AC_INIT([nm-applet],
        [1.1.93],
        [https://bugzilla.gnome.org/enter_bug.cgi?product=NetworkManager],
        [network-manager-applet])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 subdir-objects no-dist-gzip dist-xz -Wno-portability])
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

PKG_CHECK_MODULES(LIBNM_GLIB,
		[gio-2.0 >= 2.32
		 NetworkManager >= 1.1
		 libnm-glib >= 1.1
		 libnm-util >= 1.1
		 libnm-glib-vpn >= 1.1
		 gmodule-export-2.0])
LIBNM_GLIB_CFLAGS="$LIBNM_GLIB_CFLAGS -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32"
LIBNM_GLIB_CFLAGS="$LIBNM_GLIB_CFLAGS -DNM_VERSION_MIN_REQUIRED=NM_VERSION_1_2"
LIBNM_GLIB_CFLAGS="$LIBNM_GLIB_CFLAGS -DNM_VERSION_MAX_ALLOWED=NM_VERSION_1_2"

PKG_CHECK_MODULES(LIBNM, [libnm gio-2.0 >= 2.32 gmodule-export-2.0])
LIBNM_CFLAGS="$LIBNM_CFLAGS -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32"
LIBNM_CFLAGS="$LIBNM_CFLAGS -DNM_VERSION_MIN_REQUIRED=NM_VERSION_1_2"
LIBNM_CFLAGS="$LIBNM_CFLAGS -DNM_VERSION_MAX_ALLOWED=NM_VERSION_1_2"

PKG_CHECK_MODULES(LIBSECRET, [libsecret-unstable])

# Check for libnotify >= 0.7
PKG_CHECK_MODULES(LIBNOTIFY_07, [libnotify >= 0.7], [have_libnotify_07=yes],[have_libnotify_07=no])
if test x"$have_libnotify_07" = "xyes"; then
	AC_DEFINE(HAVE_LIBNOTIFY_07, 1, [Define if you have libnotify 0.7 or later])
fi
PKG_CHECK_MODULES(NOTIFY, [libnotify >= 0.4.3])

PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 3.4)
GTK_CFLAGS="$GTK_CFLAGS -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_4"

AC_ARG_WITH([appindicator], AS_HELP_STRING([--with-appindicator|--without-appindicator], [Build with libappindicator support instead of xembed systray support.]))
if test "$with_appindicator" == "yes"; then
	PKG_CHECK_MODULES(APPINDICATOR, appindicator3-0.1)
	PKG_CHECK_MODULES(DBUSMENU, dbusmenu-gtk3-0.4 >= 16.04.0)
	AC_DEFINE([WITH_APPINDICATOR], 1, [Enable using libappindicator])
fi

AM_CONDITIONAL(HAVE_GBT, test x"$have_gbt" = "xyes")

dnl ModemManager1 with libmm-glib for WWAN support
AC_ARG_WITH(wwan, AS_HELP_STRING([--with-wwan], [Enable WWAN support]),,[with_wwan=auto])
if (test "${with_wwan}" != "no"); then
    PKG_CHECK_MODULES(MM_GLIB,
                      [mm-glib],
                      [have_libmm_glib=yes],
                      [have_libmm_glib=no])

    if (test "${have_libmm_glib}" = "no"); then
        if (test "${with_wwan}" = "yes"); then
            AC_MSG_ERROR([Couldn't find libmm-glib])
        fi
    else
        with_wwan="yes"
    fi
fi

if (test "${with_wwan}" = "yes"); then
    AC_DEFINE(WITH_WWAN, 1, [Define if you have ModemManager/WWAN support])
else
	AC_DEFINE(WITH_WWAN, 0, [Define if you have ModemManager/WWAN support])
fi
AM_CONDITIONAL(WITH_WWAN, test "${with_wwan}" = "yes")

dnl Check for gudev
PKG_CHECK_MODULES(GUDEV, gudev-1.0 >= 147)

dnl Check for gobject introspection
GOBJECT_INTROSPECTION_CHECK([0.9.6])

GLIB_CONFIG_NMA
GLIB_GSETTINGS

dnl
dnl Compiler flags
dnl
NM_COMPILER_WARNINGS


AC_ARG_WITH(more-asserts,
            AS_HELP_STRING([--with-more-asserts=level], [Enable more assertions for debugging (default: 0)]),
            [more_asserts=${with_more_asserts}],
            [more_asserts=no])
if test "${more_asserts}" = "no"; then
    more_asserts=0
else
    if test "${more_asserts}" = "yes"; then
        more_asserts=100
    fi
fi
AC_DEFINE_UNQUOTED(NM_MORE_ASSERTS, $more_asserts, [Define if more asserts are enabled])


AC_CONFIG_FILES([
Makefile
src/Makefile
src/libnm-gtk/Makefile
src/libnm-gtk/tests/Makefile
src/libnm-gtk/examples/Makefile
src/libnm-gtk/libnm-gtk.pc
src/libnma/Makefile
src/libnma/libnma.pc
src/utils/Makefile
src/utils/tests/Makefile
src/wireless-security/Makefile
src/connection-editor/Makefile
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
shared/Makefile
org.gnome.nm-applet.gschema.xml
])
AC_OUTPUT
