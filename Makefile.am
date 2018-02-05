include $(GLIB_MAKEFILE)
-include $(INTROSPECTION_MAKEFILE)
include $(top_srcdir)/gtk-doc.make

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AUTOMAKE_OPTIONS = subdir-objects

BUILT_SOURCES =

bin_PROGRAMS =

lib_LTLIBRARIES =

noinst_LTLIBRARIES =

noinst_PROGRAMS =

check_PROGRAMS =

check_programs =

check_local =

TESTS =

DISTCLEANFILES =

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-dependency-tracking \
	--enable-gtk-doc \
	--enable-introspection \
	--enable-more-warnings=error \
	--with-wwan \
	--with-team \
	--with-gcr

man_MANS =

pkgconfig_DATA =

pkgconfigdir = $(libdir)/pkgconfig

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)
BUILT_SOURCES += $(gir_DATA)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)
BUILT_SOURCES += $(typelib_DATA)

INTROSPECTION_GIRS =

SUBDIRS = \
	. \
	po

###############################################################################

icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon16_DATA = \
	icons/16/nm-device-wired.png \
	icons/16/nm-no-connection.png \
	icons/16/nm-vpn-standalone-lock.png
EXTRA_DIST += $(icon16_DATA)

icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon22_DATA = \
	icons/22/nm-adhoc.png \
	icons/22/nm-device-wired.png \
	icons/22/nm-device-wwan.png \
	icons/22/nm-mb-roam.png \
	icons/22/nm-no-connection.png \
	icons/22/nm-secure-lock.png \
	icons/22/nm-signal-00.png \
	icons/22/nm-signal-100.png \
	icons/22/nm-signal-25.png \
	icons/22/nm-signal-50.png \
	icons/22/nm-signal-75.png \
	icons/22/nm-stage01-connecting01.png \
	icons/22/nm-stage01-connecting02.png \
	icons/22/nm-stage01-connecting03.png \
	icons/22/nm-stage01-connecting04.png \
	icons/22/nm-stage01-connecting05.png \
	icons/22/nm-stage01-connecting06.png \
	icons/22/nm-stage01-connecting07.png \
	icons/22/nm-stage01-connecting08.png \
	icons/22/nm-stage01-connecting09.png \
	icons/22/nm-stage01-connecting10.png \
	icons/22/nm-stage01-connecting11.png \
	icons/22/nm-stage02-connecting01.png \
	icons/22/nm-stage02-connecting02.png \
	icons/22/nm-stage02-connecting03.png \
	icons/22/nm-stage02-connecting04.png \
	icons/22/nm-stage02-connecting05.png \
	icons/22/nm-stage02-connecting06.png \
	icons/22/nm-stage02-connecting07.png \
	icons/22/nm-stage02-connecting08.png \
	icons/22/nm-stage02-connecting09.png \
	icons/22/nm-stage02-connecting10.png \
	icons/22/nm-stage02-connecting11.png \
	icons/22/nm-stage03-connecting01.png \
	icons/22/nm-stage03-connecting02.png \
	icons/22/nm-stage03-connecting03.png \
	icons/22/nm-stage03-connecting04.png \
	icons/22/nm-stage03-connecting05.png \
	icons/22/nm-stage03-connecting06.png \
	icons/22/nm-stage03-connecting07.png \
	icons/22/nm-stage03-connecting08.png \
	icons/22/nm-stage03-connecting09.png \
	icons/22/nm-stage03-connecting10.png \
	icons/22/nm-stage03-connecting11.png \
	icons/22/nm-tech-3g.png \
	icons/22/nm-tech-cdma-1x.png \
	icons/22/nm-tech-edge.png \
	icons/22/nm-tech-evdo.png \
	icons/22/nm-tech-gprs.png \
	icons/22/nm-tech-hspa.png \
	icons/22/nm-tech-lte.png \
	icons/22/nm-tech-umts.png \
	icons/22/nm-vpn-active-lock.png \
	icons/22/nm-vpn-connecting01.png \
	icons/22/nm-vpn-connecting02.png \
	icons/22/nm-vpn-connecting03.png \
	icons/22/nm-vpn-connecting04.png \
	icons/22/nm-vpn-connecting05.png \
	icons/22/nm-vpn-connecting06.png \
	icons/22/nm-vpn-connecting07.png \
	icons/22/nm-vpn-connecting08.png \
	icons/22/nm-vpn-connecting09.png \
	icons/22/nm-vpn-connecting10.png \
	icons/22/nm-vpn-connecting11.png \
	icons/22/nm-vpn-connecting12.png \
	icons/22/nm-vpn-connecting13.png \
	icons/22/nm-vpn-connecting14.png \
	icons/22/nm-wwan-tower.png
EXTRA_DIST += $(icon22_DATA)

icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon32_DATA = \
	icons/32/nm-device-wired.png \
	icons/32/nm-no-connection.png
EXTRA_DIST += $(icon32_DATA)

icon48dir = $(datadir)/icons/hicolor/48x48/apps
icon48_DATA = \
	icons/48/nm-device-wireless.png
EXTRA_DIST += $(icon48_DATA)

iconscalabledir=$(datadir)/icons/hicolor/scalable/apps
iconscalable_DATA = \
	icons/scalable/nm-device-wired.svg \
	icons/scalable/nm-no-connection.svg
EXTRA_DIST += $(iconscalable_DATA)

EXTRA_DIST += icons/meson.build

###############################################################################

shared_files = \
	shared/nm-utils/nm-shared-utils.c \
	shared/nm-utils/nm-shared-utils.h \
	$(NULL)

noinst_LTLIBRARIES += src/utils/libutils-libnm.la

src_utils_libutils_libnm_la_CPPFLAGS = \
	"-I$(srcdir)/shared" \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS)

src_utils_libutils_libnm_la_SOURCES = \
	$(shared_files) \
	src/utils/utils.c \
	src/utils/utils.h

src_utils_libutils_libnm_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBNM_LIBS)

if WITH_LIBNM_GTK

noinst_LTLIBRARIES += src/utils/libutils-libnm-glib.la

src_utils_libutils_libnm_glib_la_CPPFLAGS = \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_LEGACY \
	"-I$(srcdir)/shared" \
	$(GTK_CFLAGS) \
	$(LIBNM_GLIB_CFLAGS)

src_utils_libutils_libnm_glib_la_SOURCES = \
	$(shared_files) \
	src/utils/utils.c \
	src/utils/utils.h

src_utils_libutils_libnm_glib_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBNM_GLIB_LIBS)

endif

check_programs += src/utils/tests/test-utils

src_utils_tests_test_utils_SOURCES = src/utils/tests/test-utils.c

src_utils_tests_test_utils_CPPFLAGS = \
	"-I$(srcdir)/shared/" \
	"-I$(srcdir)/src/utils" \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS)

src_utils_tests_test_utils_LDADD = \
	src/utils/libutils-libnm.la \
	$(GTK_LIBS) \
	$(LIBNM_LIBS)

EXTRA_DIST += src/utils/meson.build

###############################################################################

src/wireless-security/ws-resources.h: src/wireless-security/ws.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/wireless-security --generate-header --internal

src/wireless-security/ws-resources.c: src/wireless-security/ws.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/src/wireless-security --generate-dependencies $(srcdir)/src/wireless-security/ws.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/wireless-security --generate-source --internal

CLEANFILES += \
	$(wireless_security_h_gen) \
	$(wireless_security_c_gen)

wireless_security_h_gen = \
	src/wireless-security/ws-resources.h

wireless_security_c_gen = \
	src/wireless-security/ws-resources.c

wireless_security_c_real = \
	src/wireless-security/wireless-security.h \
	src/wireless-security/wireless-security.c \
	src/wireless-security/ws-wep-key.h \
	src/wireless-security/ws-wep-key.c \
	src/wireless-security/ws-wpa-psk.h \
	src/wireless-security/ws-wpa-psk.c \
	src/wireless-security/ws-leap.h \
	src/wireless-security/ws-leap.c \
	src/wireless-security/ws-wpa-eap.h \
	src/wireless-security/ws-wpa-eap.c \
	src/wireless-security/ws-dynamic-wep.h \
	src/wireless-security/ws-dynamic-wep.c \
	src/wireless-security/eap-method.h \
	src/wireless-security/eap-method.c \
	src/wireless-security/eap-method-tls.h \
	src/wireless-security/eap-method-tls.c \
	src/wireless-security/eap-method-leap.h \
	src/wireless-security/eap-method-leap.c \
	src/wireless-security/eap-method-fast.h \
	src/wireless-security/eap-method-fast.c \
	src/wireless-security/eap-method-ttls.h \
	src/wireless-security/eap-method-ttls.c \
	src/wireless-security/eap-method-peap.h \
	src/wireless-security/eap-method-peap.c \
	src/wireless-security/eap-method-simple.h \
	src/wireless-security/eap-method-simple.c \
	src/wireless-security/helpers.h \
	src/wireless-security/helpers.c


noinst_LTLIBRARIES += src/wireless-security/libwireless-security-libnm.la

src_wireless_security_libwireless_security_libnm_la_SOURCES = \
	$(wireless_security_c_real)

nodist_src_wireless_security_libwireless_security_libnm_la_SOURCES = \
	$(wireless_security_c_gen)

src_wireless_security_libwireless_security_libnm_la_CPPFLAGS = \
	"-I$(srcdir)/shared" \
	"-I$(srcdir)/src/utils" \
	"-I$(srcdir)/src/libnma" \
	-Isrc/libnma \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS)

src_wireless_security_libwireless_security_libnm_la_LIBADD = \
	src/utils/libutils-libnm.la \
	$(GTK_LIBS) \
	$(LIBNM_LIBS)

$(src_wireless_security_libwireless_security_libnm_la_OBJECTS): $(wireless_security_h_gen)

if WITH_LIBNM_GTK

noinst_LTLIBRARIES += src/wireless-security/libwireless-security-libnm-glib.la

src_wireless_security_libwireless_security_libnm_glib_la_SOURCES = \
	$(wireless_security_c_real)

nodist_src_wireless_security_libwireless_security_libnm_glib_la_SOURCES = \
	$(wireless_security_c_gen)

src_wireless_security_libwireless_security_libnm_glib_la_CPPFLAGS = \
	$(GTK_CFLAGS) \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_LEGACY \
	"-I$(srcdir)/shared" \
	"-I$(srcdir)/src/utils" \
	"-I$(srcdir)/src/libnma" \
	-Isrc/libnma \
	$(LIBNM_GLIB_CFLAGS)

src_wireless_security_libwireless_security_libnm_glib_la_LIBADD = \
	src/utils/libutils-libnm-glib.la \
	$(GTK_LIBS) \
	$(LIBNM_GLIB_LIBS)

$(src_wireless_security_libwireless_security_libnm_glib_la_OBJECTS): $(wireless_security_h_gen)

endif

EXTRA_DIST += \
	src/wireless-security/eap-method-fast.ui \
	src/wireless-security/eap-method-leap.ui \
	src/wireless-security/eap-method-peap.ui \
	src/wireless-security/eap-method-simple.ui \
	src/wireless-security/eap-method-tls.ui \
	src/wireless-security/eap-method-ttls.ui \
	src/wireless-security/ws-dynamic-wep.ui \
	src/wireless-security/ws-leap.ui \
	src/wireless-security/ws-wep-key.ui \
	src/wireless-security/ws-wpa-eap.ui \
	src/wireless-security/ws-wpa-psk.ui \
	src/wireless-security/ws.gresource.xml \
	src/wireless-security/meson.build

###############################################################################

if WITH_LIBNM_GTK

src/libnm-gtk/nm-resources.h: src/libnm-gtk/nm.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/libnm-gtk --generate-header --internal

src/libnm-gtk/nm-resources.c: src/libnm-gtk/nm.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/src/libnm-gtk --generate-dependencies $(srcdir)/src/libnm-gtk/nm.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/libnm-gtk --generate-source --internal

CLEANFILES += \
	$(libnm_gtk_h_priv_gen) \
	$(libnm_gtk_c_gen)

libnm_gtk_h_priv_gen = \
	src/libnm-gtk/nm-resources.h

libnm_gtk_c_gen = \
	src/libnm-gtk/nm-resources.c

libnm_gtk_h_pub = \
	src/libnm-gtk/nm-wifi-dialog.h \
	src/libnm-gtk/nm-wireless-dialog.h \
	src/libnm-gtk/nm-mobile-wizard.h \
	src/libnm-gtk/nm-ui-utils.h \
	src/libnm-gtk/nm-mobile-providers.h \
	src/libnm-gtk/nm-vpn-password-dialog.h \
	src/libnm-gtk/nm-cert-chooser.h

libnm_gtk_c_real = \
	src/libnm-gtk/nm-wifi-dialog.c \
	src/libnm-gtk/nm-wireless-dialog.c \
	src/libnm-gtk/nm-mobile-wizard.c \
	src/libnm-gtk/nm-ui-utils.c \
	src/libnm-gtk/nm-mobile-providers.c \
	src/libnm-gtk/nm-vpn-password-dialog.c \
	src/libnma/nma-cert-chooser.c \
	src/libnma/nma-file-cert-chooser.c \
	src/libnm-gtk/init.c

src_libnm_gtk_libnmgtkdir = $(includedir)/libnm-gtk

src_libnm_gtk_libnmgtk_HEADERS = \
	$(libnm_gtk_h_pub)

lib_LTLIBRARIES += src/libnm-gtk/libnm-gtk.la

src_libnm_gtk_libnm_gtk_la_SOURCES = \
	$(libnm_gtk_c_real)

nodist_src_libnm_gtk_libnm_gtk_la_SOURCES = \
	$(libnm_gtk_c_gen)

src_libnm_gtk_libnm_gtk_la_CFLAGS = \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB_LEGACY \
	-DICONDIR=\""$(datadir)/icons"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DAUTOSTARTDIR=\""$(sysconfdir)/xdg/autostart"\" \
	-DNMALOCALEDIR=\"$(datadir)/locale\" \
	"-I$(srcdir)/shared" \
	"-I$(srcdir)/src/utils" \
	"-I$(srcdir)/src/wireless-security" \
	"-I$(srcdir)/src/libnm-gtk" \
	"-I$(srcdir)/src/libnma" \
	-Isrc/libnma \
	$(GTK_CFLAGS) \
	$(LIBNM_GLIB_CFLAGS) \
	$(GUDEV_CFLAGS)

src_libnm_gtk_libnm_gtk_la_LIBADD = \
	src/wireless-security/libwireless-security-libnm-glib.la \
	$(GTK_LIBS) \
	$(LIBNM_GLIB_LIBS) \
	$(GUDEV_LIBS)

EXTRA_src_libnm_gtk_libnm_gtk_la_DEPENDENCIES = src/libnm-gtk/libnm-gtk.ver

src_libnm_gtk_libnm_gtk_la_LDFLAGS = \
	-Wl,-no-undefined \
	-Wl,--version-script=$(srcdir)/src/libnm-gtk/libnm-gtk.ver

$(src_libnm_gtk_libnm_gtk_la_OBJECTS): $(libnm_gtk_h_priv_gen)

check-local-exports-libnm-gtk:
	$(srcdir)/src/libnm-gtk/check-exports.sh $(builddir)/src/libnm-gtk/.libs/libnm-gtk.so $(srcdir)/src/libnm-gtk/libnm-gtk.ver

check_local += check-local-exports-libnm-gtk


pkgconfig_DATA += src/libnm-gtk/libnm-gtk.pc

if HAVE_INTROSPECTION
src/libnm-gtk/NMGtk-1.0.gir: src/libnm-gtk/libnm-gtk.la
src_libnm_gtk_NMGtk_1_0_gir_INCLUDES = NMClient-1.0 NetworkManager-1.0 Gtk-3.0
src_libnm_gtk_NMGtk_1_0_gir_EXPORT_PACKAGES = libnm-gtk
src_libnm_gtk_NMGtk_1_0_gir_CFLAGS = $(src_libnm_gtk_libnm_gtk_la_CFLAGS)
src_libnm_gtk_NMGtk_1_0_gir_LIBS = src/libnm-gtk/libnm-gtk.la
src_libnm_gtk_NMGtk_1_0_gir_FILES = \
	$(filter-out src/libnm-gtk/nm-wireless-dialog.h, $(libnm_gtk_h_pub)) \
	$(filter-out src/libnm-gtk/nm-wireless-dialog.c, $(libnm_gtk_c_real))
src_libnm_gtk_NMGtk_1_0_gir_SCANNERFLAGS = --warn-all --identifier-prefix=NMA --symbol-prefix=nma
INTROSPECTION_GIRS += src/libnm-gtk/NMGtk-1.0.gir
endif


check_programs += src/libnm-gtk/tests/test-mobile-providers

src_libnm_gtk_tests_test_mobile_providers_CPPFLAGS = \
	-DTEST_DATA_DIR=\""$(abs_srcdir)/src/libnm-gtk/tests"\" \
	"-I$(srcdir)/shared" \
	"-I$(srcdir)/src/libnm-gtk" \
	$(GTK_CFLAGS) \
	$(LIBNM_GLIB_CFLAGS)

src_libnm_gtk_tests_test_mobile_providers_LDADD = \
	src/libnm-gtk/libnm-gtk.la \
	$(LIBNM_GLIB_LIBS)

endif

EXTRA_DIST += \
	src/libnm-gtk/libnm-gtk.pc.in \
	src/libnm-gtk/libnm-gtk.ver \
	src/libnm-gtk/check-exports.sh \
	src/libnm-gtk/wifi.ui \
	src/libnm-gtk/nm.gresource.xml \
	src/libnm-gtk/meson.build \
	\
	src/libnm-gtk/tests/iso3166-test.xml \
	src/libnm-gtk/tests/serviceproviders-test.xml \
	\
	src/libnm-gtk/examples/mobile-providers.js \
	$(NULL)

###############################################################################

src/libnma/nma-resources.h: src/libnma/nma.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/libnma --generate-header --internal

src/libnma/nma-resources.c: src/libnma/nma.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/src/libnma --generate-dependencies $(srcdir)/src/libnma/nma.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/libnma --generate-source --internal

CLEANFILES += \
	$(libnma_h_priv_gen) \
	$(libnma_c_gen)

libnma_h_priv_gen = \
	src/libnma/nma-resources.h

libnma_c_gen = \
	src/libnma/nma-resources.c

libnma_h_pub = \
	src/libnma/nma-version.h \
	src/libnma/nma-wifi-dialog.h \
	src/libnma/nma-mobile-wizard.h \
	src/libnma/nma-mobile-providers.h \
	src/libnma/nma-vpn-password-dialog.h \
	src/libnma/nma-ui-utils.h \
	src/libnma/nma-cert-chooser.h

libnma_c_real = \
	src/libnma/nma-wifi-dialog.c \
	src/libnma/nma-mobile-wizard.c \
	src/libnma/nma-mobile-providers.c \
	src/libnma/nma-vpn-password-dialog.c \
	src/libnma/nma-ui-utils.c \
	src/libnma/init.c \
	src/libnma/nma-cert-chooser.c \
	src/libnma/nma-file-cert-chooser.c

libnma_h_priv_real = \
	src/libnma/nma-cert-chooser-private.h

if WITH_GCR
libnma_h_priv_real += \
	src/libnma/nma-pkcs11-token-login-dialog.h \
	src/libnma/nma-pkcs11-cert-chooser-dialog.h \
	src/libnma/nma-cert-chooser-button.h

libnma_c_real += \
	src/libnma/nma-pkcs11-token-login-dialog.c \
	src/libnma/nma-pkcs11-cert-chooser-dialog.c \
	src/libnma/nma-cert-chooser-button.c \
	src/libnma/nma-pkcs11-cert-chooser.c
endif

src_libnma_libnmadir = $(includedir)/libnma

src_libnma_libnma_HEADERS = \
	$(libnma_h_pub)

lib_LTLIBRARIES += src/libnma/libnma.la

src_libnma_libnma_la_SOURCES = \
	$(libnma_h_priv_real) \
	$(libnma_c_real)

nodist_src_libnma_libnma_la_SOURCES = \
	$(libnma_h_priv_gen) \
	$(libnma_c_gen)

src_libnma_libnma_la_CFLAGS = \
	-DICONDIR=\""$(datadir)/icons"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DAUTOSTARTDIR=\""$(sysconfdir)/xdg/autostart"\" \
	-DNMALOCALEDIR=\"$(datadir)/locale\" \
	-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_LIB \
	"-I$(srcdir)/shared" \
	"-I$(srcdir)/src/utils" \
	"-I$(srcdir)/src/wireless-security" \
	"-I$(srcdir)/src/libnma" \
	-Isrc/libnma \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	$(GUDEV_CFLAGS) \
	$(GCR_CFLAGS)

src_libnma_libnma_la_LIBADD = \
	src/wireless-security/libwireless-security-libnm.la \
	$(GTK_LIBS) \
	$(LIBNM_LIBS) \
	$(GUDEV_LIBS) \
	$(GCR_LIBS)

EXTRA_src_libnma_libnma_la_DEPENDENCIES = src/libnma/libnma.ver

src_libnma_libnma_la_LDFLAGS = \
	-Wl,-no-undefined \
	-Wl,--version-script=$(srcdir)/src/libnma/libnma.ver


pkgconfig_DATA += src/libnma/libnma.pc

if HAVE_INTROSPECTION
src/libnma/NMA-1.0.gir: src/libnma/libnma.la
src_libnma_NMA_1_0_gir_INCLUDES = NM-1.0 Gtk-3.0
src_libnma_NMA_1_0_gir_EXPORT_PACKAGES = libnma
src_libnma_NMA_1_0_gir_CFLAGS = $(src_libnma_libnma_la_CFLAGS)
src_libnma_NMA_1_0_gir_LIBS = src/libnma/libnma.la
src_libnma_NMA_1_0_gir_FILES = \
	$(libnma_h_pub) \
	$(libnma_c_real)
src_libnma_NMA_1_0_gir_SCANNERFLAGS = --warn-all --identifier-prefix=NMA --symbol-prefix=nma
INTROSPECTION_GIRS += src/libnma/NMA-1.0.gir
endif

EXTRA_DIST += \
	src/libnma/nma-version.h.in \
	src/libnma/libnma.pc.in \
	src/libnma/libnma.ver \
	src/libnma/wifi.ui \
	src/libnma/nma-pkcs11-token-login-dialog.ui \
	src/libnma/nma-pkcs11-cert-chooser-dialog.ui \
	src/libnma/nma.gresource.xml \
	src/libnma/meson.build

###############################################################################

src/connection-editor/ce-resources.h: src/connection-editor/ce.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/connection-editor --generate-header --internal

src/connection-editor/ce-resources.c: src/connection-editor/ce.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/src/connection-editor --generate-dependencies $(srcdir)/src/connection-editor/ce.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src/connection-editor --generate-source --internal

CLEANFILES += \
	$(connection_editor_h_gen) \
	$(connection_editor_c_gen)

connection_editor_h_gen = \
	src/connection-editor/ce-resources.h

connection_editor_c_gen = \
	src/connection-editor/ce-resources.c

connection_editor_hc_real = \
	src/connection-editor/nm-connection-editor.c \
	src/connection-editor/nm-connection-editor.h \
	src/connection-editor/nm-connection-list.c \
	src/connection-editor/nm-connection-list.h \
	src/connection-editor/main.c \
	src/connection-editor/ce-page.h \
	src/connection-editor/ce-page.c \
	src/connection-editor/page-general.h \
	src/connection-editor/page-general.c \
	src/connection-editor/page-ethernet.h \
	src/connection-editor/page-ethernet.c \
	src/connection-editor/page-8021x-security.h \
	src/connection-editor/page-8021x-security.c \
	src/connection-editor/page-wifi.h \
	src/connection-editor/page-wifi.c \
	src/connection-editor/page-wifi-security.h \
	src/connection-editor/page-wifi-security.c \
	src/connection-editor/page-infiniband.h \
	src/connection-editor/page-infiniband.c \
	src/connection-editor/page-ip-tunnel.h \
	src/connection-editor/page-ip-tunnel.c \
	src/connection-editor/page-ip4.h \
	src/connection-editor/page-ip4.c \
	src/connection-editor/page-ip6.h \
	src/connection-editor/page-ip6.c \
	src/connection-editor/page-dsl.h \
	src/connection-editor/page-dsl.c \
	src/connection-editor/page-macsec.h \
	src/connection-editor/page-macsec.c \
	src/connection-editor/page-mobile.h \
	src/connection-editor/page-mobile.c \
	src/connection-editor/page-bluetooth.h \
	src/connection-editor/page-bluetooth.c \
	src/connection-editor/page-ppp.h \
	src/connection-editor/page-ppp.c \
	src/connection-editor/page-proxy.h \
	src/connection-editor/page-proxy.c \
	src/connection-editor/page-vpn.h \
	src/connection-editor/page-vpn.c \
	src/connection-editor/page-master.h \
	src/connection-editor/page-master.c \
	src/connection-editor/page-bond.h \
	src/connection-editor/page-bond.c \
	src/connection-editor/page-team.h \
	src/connection-editor/page-team.c \
	src/connection-editor/page-team-port.h \
	src/connection-editor/page-team-port.c \
	src/connection-editor/page-bridge.h \
	src/connection-editor/page-bridge.c \
	src/connection-editor/page-bridge-port.h \
	src/connection-editor/page-bridge-port.c \
	src/connection-editor/page-vlan.h \
	src/connection-editor/page-vlan.c \
	src/connection-editor/page-dcb.c \
	src/connection-editor/page-dcb.h \
	src/connection-editor/vpn-helpers.h \
	src/connection-editor/vpn-helpers.c \
	src/connection-editor/ip4-routes-dialog.h \
	src/connection-editor/ip4-routes-dialog.c \
	src/connection-editor/ip6-routes-dialog.h \
	src/connection-editor/ip6-routes-dialog.c \
	src/connection-editor/ppp-auth-methods-dialog.c \
	src/connection-editor/ppp-auth-methods-dialog.h \
	src/connection-editor/ce-polkit-button.c \
	src/connection-editor/ce-polkit-button.h \
	src/connection-editor/ce-polkit.c \
	src/connection-editor/ce-polkit.h \
	src/connection-editor/connection-helpers.c \
	src/connection-editor/connection-helpers.h

bin_PROGRAMS += src/connection-editor/nm-connection-editor

src_connection_editor_nm_connection_editor_SOURCES = \
	$(connection_editor_hc_real)

nodist_src_connection_editor_nm_connection_editor_SOURCES = \
	$(connection_editor_c_gen)

src_connection_editor_nm_connection_editor_CPPFLAGS = \
	-DICONDIR=\""$(datadir)/icons"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DNMALOCALEDIR=\"$(datadir)/locale\" \
	"-I$(srcdir)/shared" \
	"-I$(srcdir)/src/utils" \
	"-I$(srcdir)/src/wireless-security" \
	"-I$(srcdir)/src/libnma" \
	-Isrc/libnma \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	$(JANSSON_CFLAGS) \
	$(SELINUX_CFLAGS)

src_connection_editor_nm_connection_editor_LDADD = \
	src/wireless-security/libwireless-security-libnm.la \
	src/libnma/libnma.la \
	$(GTK_LIBS) \
	$(LIBNM_LIBS) \
	$(JANSSON_LIBS) \
	$(SELINUX_LIBS) \
	-lm

$(src_connection_editor_nm_connection_editor_OBJECTS): $(connection_editor_h_gen)

EXTRA_src_connection_editor_nm_connection_editor_DEPENDENCIES = linker-script-binary.ver

src_connection_editor_nm_connection_editor_LDFLAGS = \
	-Wl,--version-script="$(srcdir)/linker-script-binary.ver"


EXTRA_DIST += \
	src/connection-editor/ce-ip4-routes.ui \
	src/connection-editor/ce-ip6-routes.ui \
	src/connection-editor/ce-new-connection.ui \
	src/connection-editor/ce-page-bluetooth.ui \
	src/connection-editor/ce-page-bond.ui \
	src/connection-editor/ce-page-bridge-port.ui \
	src/connection-editor/ce-page-bridge.ui \
	src/connection-editor/ce-page-dcb.ui \
	src/connection-editor/ce-page-dsl.ui \
	src/connection-editor/ce-page-ethernet.ui \
	src/connection-editor/ce-page-general.ui \
	src/connection-editor/ce-page-infiniband.ui \
	src/connection-editor/ce-page-ip4.ui \
	src/connection-editor/ce-page-ip6.ui \
	src/connection-editor/ce-page-ip-tunnel.ui \
	src/connection-editor/ce-page-macsec.ui \
	src/connection-editor/ce-page-mobile.ui \
	src/connection-editor/ce-page-ppp.ui \
	src/connection-editor/ce-page-proxy.ui \
	src/connection-editor/ce-page-team-port.ui \
	src/connection-editor/ce-page-team.ui \
	src/connection-editor/ce-page-vlan.ui \
	src/connection-editor/ce-page-wifi-security.ui \
	src/connection-editor/ce-page-wifi.ui \
	src/connection-editor/ce-ppp-auth-methods.ui \
	src/connection-editor/nm-connection-editor.ui \
	src/connection-editor/nm-connection-list.ui \
	src/connection-editor/ce.gresource.xml \
	src/connection-editor/meson.build

###############################################################################

src/applet-resources.h: src/applet.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src --generate-header --internal

src/applet-resources.c: src/applet.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/src --generate-dependencies $(srcdir)/src/applet.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/src --generate-source --internal

CLEANFILES += \
	$(nm_applet_h_gen) \
	$(nm_applet_c_gen)

nm_applet_h_gen = \
	src/applet-resources.h

nm_applet_c_gen = \
	src/applet-resources.c

nm_applet_hc_real = \
	shared/nm-utils/nm-compat.c \
	src/main.c \
	src/applet.c \
	src/applet.h \
	src/applet-agent.c \
	src/applet-agent.h \
	src/applet-vpn-request.c \
	src/applet-vpn-request.h \
	src/ethernet-dialog.h \
	src/ethernet-dialog.c \
	src/applet-dialogs.h \
	src/applet-dialogs.c \
	src/applet-device-ethernet.h \
	src/applet-device-ethernet.c \
	src/applet-device-wifi.h \
	src/applet-device-wifi.c \
	src/ap-menu-item.h \
	src/ap-menu-item.c \
	src/mb-menu-item.h \
	src/mb-menu-item.c \
	src/mobile-helpers.c \
	src/mobile-helpers.h \
	src/applet-device-bt.h \
	src/applet-device-bt.c \
	src/fallback-icon.h

if WITH_WWAN
nm_applet_hc_real += \
	src/applet-device-broadband.h \
	src/applet-device-broadband.c
endif

bin_PROGRAMS += src/nm-applet

src_nm_applet_SOURCES = \
	$(nm_applet_hc_real)

nodist_src_nm_applet_SOURCES = \
	$(nm_applet_c_gen)

src_nm_applet_CPPFLAGS = \
	-DICONDIR=\""$(datadir)/icons"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-DAUTOSTARTDIR=\""$(sysconfdir)/xdg/autostart"\" \
	-DNMALOCALEDIR=\"$(datadir)/locale\" \
	-DG_LOG_DOMAIN=\""nm-applet"\" \
	"-I$(srcdir)/shared" \
	"-I$(srcdir)/src/utils" \
	"-I$(srcdir)/src/wireless-security" \
	"-I$(srcdir)/src/libnma" \
	-Isrc/libnma \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	$(LIBSECRET_CFLAGS) \
	$(NOTIFY_CFLAGS) \
	$(MM_GLIB_CFLAGS) \
	$(APPINDICATOR_CFLAGS)

src_nm_applet_LDADD = \
	src/wireless-security/libwireless-security-libnm.la \
	src/libnma/libnma.la \
	$(GTK_LIBS) \
	$(LIBNM_LIBS) \
	$(LIBSECRET_LIBS) \
	$(NOTIFY_LIBS) \
	$(MM_GLIB_LIBS) \
	$(APPINDICATOR_LIBS) \
	-lm

$(src_nm_applet_OBJECTS): $(nm_applet_h_gen)

EXTRA_src_nm_applet_DEPENDENCIES = linker-script-binary.ver

src_nm_applet_LDFLAGS = \
	-Wl,--version-script="$(srcdir)/linker-script-binary.ver"


EXTRA_DIST += \
	src/8021x.ui \
	src/gsm-unlock.ui \
	src/info.ui \
	src/applet.gresource.xml \
	src/meson.build

###############################################################################

EXTRA_DIST += \
	shared/nm-utils/gsystem-local-alloc.h \
	shared/nm-utils/nm-compat.c \
	shared/nm-utils/nm-compat.h \
	shared/nm-utils/nm-glib.h \
	shared/nm-utils/nm-macros-internal.h \
	shared/nm-utils/nm-shared-utils.c \
	shared/nm-utils/nm-shared-utils.h \
	shared/nm-utils/nm-test-utils.h \
	shared/nm-utils/nm-vpn-editor-plugin-call.h \
	shared/nm-default.h \
	shared/nm-libnm-compat.h \
	shared/meson.build \
	$(NULL)

###############################################################################

man_MANS += \
	man/nm-applet.1 \
	man/nm-connection-editor.1

EXTRA_DIST += man/meson.build

###############################################################################

DOC_MODULE = libnma

DOC_SOURCE_DIR = $(top_srcdir)/src/libnma

SCAN_OPTIONS=--rebuild-types --rebuild-sections

DOC_MAIN_SGML_FILE = libnma-docs.xml

GTKDOC_LIBS = src/libnma/libnma.la

scan-build.stamp: $(GTKDOC_LIBS)

HFILE_GLOB = $(top_srcdir)/src/libnma/*.h

CFILE_GLOB = $(top_srcdir)/src/libnma/*.c

IGNORE_HFILES = \
	nma-resources.h \
	nma-version.h \
	nma-cert-chooser-private.h \
	nma-pkcs11-token-login-dialog.h \
	nma-pkcs11-cert-chooser-dialog.h \
	nma-cert-chooser-button.h

mkdb_ignore_c_files = \
	nma-resources.c \
	nma-file-cert-chooser.c \
	nma-pkcs11-token-login-dialog.c \
	nma-pkcs11-cert-chooser-dialog.c \
	nma-cert-chooser-button.c \
	nma-pkcs11-cert-chooser.c

MKDB_OPTIONS = --ignore-files "$(IGNORE_HFILES) $(mkdb_ignore_c_files)"

###############################################################################

check_PROGRAMS += $(check_programs)

check-local: $(check_local)

TESTS += $(check_programs)

EXTRA_DIST += \
	linker-script-binary.ver \
	CONTRIBUTING \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	Makefile.glib \
	autogen.sh \
	meson.build \
	meson_options.txt \
	meson_post_install.py \
	config.h.meson \
	gtkdocentities.ent.in \
	\
	po/meson.build


autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = nm-applet.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

applicationsdir = $(datadir)/applications
applications_DATA = $(autostart_DATA)

desktopdir = $(datadir)/applications
desktop_in_files = nm-connection-editor.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

gsettings_SCHEMAS = org.gnome.nm-applet.gschema.xml
@GSETTINGS_RULES@

appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = nm-connection-editor.appdata.xml.in
@INTLTOOL_XML_RULE@

convertdir=$(datadir)/GConf/gsettings
convert_DATA=nm-applet.convert

EXTRA_DIST += \
	$(convert_DATA) \
	$(appdata_in_files) \
	$(autostart_in_files) \
	$(desktop_in_files)

CLEANFILES += \
	$(INTROSPECTION_GIRS) \
	$(typelib_DATA) \
	\
	$(autostart_DATA) \
	$(desktop_DATA) \
	$(appdata_DATA) \
	$(BUILT_SOURCES)

DISTCLEANFILES += \
	intltool-extract \
	intltool-merge \
	intltool-update
