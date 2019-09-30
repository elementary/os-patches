AC_PREREQ(2.52)

m4_define(gnomebt_version_major, 3)
m4_define(gnomebt_version_minor, 8)
m4_define(gnomebt_version_micro, 2.1)

AC_INIT([gnome-bluetooth],
        [gnomebt_version_major.gnomebt_version_minor.gnomebt_version_micro],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-bluetooth])

AC_CONFIG_SRCDIR([applet/bluetooth-applet.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.8 dist-xz no-dist-gzip check-news tar-ustar])

# Before making a release, the GNOMEBT_LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
GNOMEBT_LT_VERSION=11:0:0
AC_SUBST(GNOMEBT_LT_VERSION)

AM_MAINTAINER_MODE

# Support silent build rules, requires at least automake-1.11. Disable
# by either passing --disable-silent-rules to configure or passing V=1
# to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_LANG_C
AC_PROG_CC
AC_PROG_INSTALL

dnl Initialize libtool
AM_DISABLE_STATIC
AM_PROG_LIBTOOL

# i18n stuff
IT_PROG_INTLTOOL([0.40.0])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

GETTEXT_PACKAGE=gnome-bluetooth2
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
					[The name of the gettext domain])

GLIB_GSETTINGS

AC_PATH_PROG([XMLLINT],[xmllint])

dnl gtk-doc checks
GTK_DOC_CHECK(1.9)

AC_DEFINE_UNQUOTED(LOCALEDIR, "${prefix}/share/locale", [Directory for the localization files])

AC_ARG_ENABLE(desktop-update, AC_HELP_STRING([--disable-desktop-update],
					[Disable desktop database update]))
if (test "$enable_desktop_update" != no); then
	AC_PATH_PROG(UPDATE_DESKTOP_DATABASE, [update-desktop-database])
fi
AM_CONDITIONAL([DESKTOP_UPDATE], [test -n "$UPDATE_DESKTOP_DATABASE"])

AC_ARG_ENABLE(icon-update, AC_HELP_STRING([--disable-icon-update],
					[Disable icon cache update]))
if (test "$enable_icon_update" != no); then
	AC_PATH_PROG(UPDATE_ICON_CACHE, [gtk-update-icon-cache])
fi
AM_CONDITIONAL([ICON_UPDATE], [test -n "$UPDATE_ICON_CACHE"])

GTK_REQUIRED=2.91.3
GLIB_REQUIRED=2.29.90
NOTIFY_REQUIRED=0.7.0
NAUTILUS_SENDTO_REQUIRED=2.29.0
NAUTILUS_SENDTO_MAX_REQUIRED=3.7.0

dnl Requires for the sendto app
PKG_CHECK_MODULES(SENDTO,
		  gtk+-3.0
		  gio-2.0)

dnl Requires for the wizard dialogue
PKG_CHECK_MODULES(WIZARD,
		  gmodule-export-2.0
		  gtk+-3.0)

AC_CHECK_LIBM
AC_SUBST(LIBM)

dnl Requires for the input helper
PKG_CHECK_MODULES(INPUT,
		  xi
		  x11
		  gtk+-3.0)

dnl Requires for the public library
PKG_CHECK_MODULES(LIBGNOMEBT, gmodule-2.0 gio-unix-2.0 gtk+-3.0)

dnl Requires for the geoclue plugin
PKG_CHECK_MODULES(GEOCLUE, gio-2.0 gtk+-3.0)

dnl Requires for the plugins
PKG_CHECK_MODULES(PLUGINS,
		  gtk+-3.0)

GDBUS_CODEGEN="gdbus-codegen"
AC_SUBST(GDBUS_CODEGEN)

GOBJECT_INTROSPECTION_CHECK([0.9.5])

GNOME_COMMON_INIT
GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_CXX_WARNINGS
GNOME_MAINTAINER_MODE_DEFINES

AC_ARG_ENABLE([documentation],
	AS_HELP_STRING([--enable-documentation], [enable man pages and HTML]),
	[], [enable_documentation=yes])
AM_CONDITIONAL(ENABLE_DOCUMENTATION, test x$enable_documentation = xyes)

YELP_HELP_INIT

AC_OUTPUT(Makefile
	  gnome-bluetooth-1.0.pc
	  icons/Makefile
	  lib/Makefile
	  lib/plugins/Makefile
	  applet/Makefile
	  wizard/Makefile
	  wizard/bluetooth-wizard.desktop.in
	  sendto/Makefile
	  sendto/bluetooth-sendto.desktop.in
	  docs/Makefile
	  docs/reference/Makefile
	  docs/reference/libgnome-bluetooth/Makefile
	  docs/reference/libgnome-bluetooth/version.xml
	  po/Makefile.in
	  help/Makefile
)

echo "
Configure summary:

	Compiler....................:  ${CC}
	Compiler Flags..............:  ${CFLAGS}
	Prefix......................:  ${prefix}
	Documentation...............:  ${enable_gtk_doc}
	GObject-Introspection.......:  ${found_introspection}
"

