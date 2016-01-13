SUBDIRS = . tests examples

uidir = $(datadir)/libnm-gtk
ui_DATA = wifi.ui

libnmgtkdir = $(includedir)/libnm-gtk

libnmgtk_HEADERS = \
	nm-wifi-dialog.h \
	nm-wireless-dialog.h \
	nm-mobile-wizard.h \
	nm-ui-utils.h \
	nm-mobile-providers.h \
	nm-vpn-password-dialog.h

lib_LTLIBRARIES = libnm-gtk.la

libnm_gtk_la_SOURCES = \
	nm-wifi-dialog.c \
	nm-wireless-dialog.c \
	nm-mobile-wizard.c \
	nm-ui-utils.c \
	nm-mobile-providers.c \
	nm-vpn-password-dialog.c \
	init.c

libnm_gtk_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(NMA_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GUDEV_CFLAGS) \
	-DICONDIR=\""$(datadir)/icons"\" \
	-DUIDIR=\""$(uidir)"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DAUTOSTARTDIR=\""$(sysconfdir)/xdg/autostart"\" \
	-DVPN_NAME_FILES_DIR=\""$(sysconfdir)/NetworkManager/VPN"\" \
	-DNMALOCALEDIR=\"$(datadir)/locale\" \
	$(DISABLE_DEPRECATED) \
	-I${top_builddir}/src/marshallers \
	-I${top_srcdir}/src/utils \
	-I${top_srcdir}/src/wireless-security

libnm_gtk_la_LIBADD = \
	$(GTK_LIBS) \
	$(NMA_LIBS) \
	$(GUDEV_LIBS) \
	$(top_builddir)/src/marshallers/libmarshallers.la \
	$(top_builddir)/src/wireless-security/libwireless-security.la

EXTRA_libnm_gtk_la_DEPENDENCIES = $(srcdir)/libnm-gtk.ver

libnm_gtk_la_LDFLAGS = -Wl,-no-undefined \
	-Wl,--version-script=$(srcdir)/libnm-gtk.ver

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libnm-gtk.pc

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
introspection_sources =			\
	$(filter-out			\
		nm-wireless-dialog.%,	\
		$(libnmgtk_HEADERS) $(libnm_gtk_la_SOURCES))

NMGtk-1.0.gir: libnm-gtk.la
NMGtk_1_0_gir_INCLUDES = NMClient-1.0 NetworkManager-1.0 Gtk-3.0
NMGtk_1_0_gir_EXPORT_PACKAGES = libnm-gtk
NMGtk_1_0_gir_CFLAGS = $(libnm_gtk_la_CFLAGS)
NMGtk_1_0_gir_LIBS = libnm-gtk.la
NMGtk_1_0_gir_FILES = $(introspection_sources)
NMGtk_1_0_gir_SCANNERFLAGS = --warn-all --identifier-prefix=NMA --symbol-prefix=nma
INTROSPECTION_GIRS += NMGtk-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

check-local:
	$(top_srcdir)/src/libnm-gtk/check-exports.sh $(builddir)/.libs/libnm-gtk.so $(srcdir)/libnm-gtk.ver

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

EXTRA_DIST = libnm-gtk.pc.in libnm-gtk.ver check-exports.sh $(ui_DATA)
