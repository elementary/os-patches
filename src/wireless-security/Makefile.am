noinst_LTLIBRARIES = libwireless-security.la

libwireless_security_la_SOURCES = \
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

libwireless_security_la_CPPFLAGS = \
	$(GTK_CFLAGS) \
	-DUIDIR=\""$(uidir)"\" \
	$(NMA_CFLAGS) \
	$(DISABLE_DEPRECATED) \
	-I${top_srcdir}/src/utils \
	-I${top_srcdir}/src/libnm-gtk

libwireless_security_la_LIBADD = \
	$(GTK_LIBS) \
	$(NMA_LIBS) \
	${top_builddir}/src/utils/libutils.la

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

