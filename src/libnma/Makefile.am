uidir = $(datadir)/libnma
ui_DATA = wifi.ui

libnmadir = $(includedir)/libnma

libnma_HEADERS = \
	nma-wifi-dialog.h \
	nma-mobile-wizard.h \
	nma-mobile-providers.h \
	nma-vpn-password-dialog.h \
	nma-ui-utils.h

lib_LTLIBRARIES = libnma.la

libnma_la_SOURCES = \
	nma-wifi-dialog.c \
	nma-mobile-wizard.c \
	nma-mobile-providers.c \
	nma-vpn-password-dialog.c \
	nma-ui-utils.c \
	init.c

libnma_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	$(GUDEV_CFLAGS) \
	-DLIBNM_BUILD \
	-DICONDIR=\""$(datadir)/icons"\" \
	-DUIDIR=\""$(uidir)"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DAUTOSTARTDIR=\""$(sysconfdir)/xdg/autostart"\" \
	-DVPN_NAME_FILES_DIR=\""$(sysconfdir)/NetworkManager/VPN"\" \
	-DNMALOCALEDIR=\"$(datadir)/locale\" \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB \
	"-I${top_srcdir}/shared/" \
	-I${top_srcdir}/src/utils \
	-I${top_srcdir}/src/wireless-security

libnma_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBNM_LIBS) \
	$(GUDEV_LIBS) \
	$(top_builddir)/src/wireless-security/libwireless-security-libnm.la

EXTRA_libnma_la_DEPENDENCIES = $(srcdir)/libnma.ver

libnma_la_LDFLAGS = -Wl,-no-undefined \
	-Wl,--version-script=$(srcdir)/libnma.ver


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libnma.pc

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =

if HAVE_INTROSPECTION
introspection_sources = $(libnma_HEADERS) $(libnma_la_SOURCES)

NMA-1.0.gir: libnma.la
NMA_1_0_gir_INCLUDES = NMClient-1.0 NetworkManager-1.0 Gtk-3.0
NMA_1_0_gir_EXPORT_PACKAGES = libnma
NMA_1_0_gir_CFLAGS = $(libnma_la_CFLAGS)
NMA_1_0_gir_LIBS = libnma.la
NMA_1_0_gir_FILES = $(introspection_sources)
NMA_1_0_gir_SCANNERFLAGS = --warn-all --identifier-prefix=NMA --symbol-prefix=nma
INTROSPECTION_GIRS += NMA-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

EXTRA_DIST = libnma.pc.in libnma.ver $(ui_DATA)
