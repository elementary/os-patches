noinst_LTLIBRARIES = \
	libwireless-security-libnm-glib.la \
	libwireless-security-libnm.la

wireless_security_sources = \
	wireless-security.h \
	wireless-security.c \
	ws-wep-key.h \
	ws-wep-key.c \
	ws-wpa-psk.h \
	ws-wpa-psk.c \
	ws-leap.h \
	ws-leap.c \
	ws-wpa-eap.h \
	ws-wpa-eap.c \
	ws-dynamic-wep.h \
	ws-dynamic-wep.c \
	eap-method.h \
	eap-method.c \
	eap-method-tls.h \
	eap-method-tls.c \
	eap-method-leap.h \
	eap-method-leap.c \
	eap-method-fast.h \
	eap-method-fast.c \
	eap-method-ttls.h \
	eap-method-ttls.c \
	eap-method-peap.h \
	eap-method-peap.c \
	eap-method-simple.h \
	eap-method-simple.c \
	helpers.h \
	helpers.c

libwireless_security_libnm_glib_la_SOURCES = \
	$(wireless_security_sources)

libwireless_security_libnm_glib_la_CPPFLAGS = \
	$(GTK_CFLAGS) \
	-DUIDIR=\""$(uidir)"\" \
	-DLIBNM_GLIB_BUILD \
	$(LIBNM_GLIB_CFLAGS) \
	"-I${top_srcdir}/shared/" \
	-I${top_srcdir}/src/utils \
	-I${top_srcdir}/src/libnma

libwireless_security_libnm_glib_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBNM_GLIB_LIBS) \
	${top_builddir}/src/utils/libutils-libnm-glib.la

libwireless_security_libnm_la_SOURCES = \
	$(wireless_security_sources)

libwireless_security_libnm_la_CPPFLAGS = \
	$(GTK_CFLAGS) \
	-DUIDIR=\""$(uidir)"\" \
	-DLIBNM_BUILD \
	$(LIBNM_CFLAGS) \
	"-I${top_srcdir}/shared/" \
	-I${top_srcdir}/src/utils \
	-I${top_srcdir}/src/libnma

libwireless_security_libnm_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBNM_LIBS) \
	${top_builddir}/src/utils/libutils-libnm.la

uidir = $(datadir)/nm-applet
ui_DATA = \
	eap-method-leap.ui \
	eap-method-fast.ui \
	eap-method-peap.ui \
	eap-method-simple.ui \
	eap-method-tls.ui \
	eap-method-ttls.ui \
	ws-dynamic-wep.ui \
	ws-leap.ui \
	ws-wep-key.ui \
	ws-wpa-eap.ui \
	ws-wpa-psk.ui

EXTRA_DIST = \
	$(ui_DATA)

