bin_PROGRAMS = nm-connection-editor

nm_connection_editor_CPPFLAGS = \
	$(GTK_CFLAGS) \
	$(NMA_CFLAGS) \
	-DNM_VERSION_MIN_REQUIRED=NM_VERSION_1_0 \
	-DNM_VERSION_MAX_ALLOWED=NM_VERSION_1_0 \
	-DICONDIR=\""$(datadir)/icons"\" \
	-DUIDIR=\""$(uidir)"\" \
	-DBINDIR=\""$(bindir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DNMALOCALEDIR=\"$(datadir)/locale\" \
	$(DBUS_CFLAGS) \
	$(DISABLE_DEPRECATED) \
	-I${top_srcdir}/src/utils \
	-I${top_srcdir}/src/wireless-security \
	-I${top_srcdir}/src/libnm-gtk \
	-I${top_builddir}/src/marshallers

nm_connection_editor_SOURCES = \
	nm-connection-editor.c \
	nm-connection-editor.h \
	nm-connection-list.c \
	nm-connection-list.h \
	main.c \
	ce-page.h \
	ce-page.c \
	page-general.h \
	page-general.c \
	page-ethernet.h \
	page-ethernet.c \
	page-8021x-security.h \
	page-8021x-security.c \
	page-wifi.h \
	page-wifi.c \
	page-wifi-security.h \
	page-wifi-security.c \
	page-wimax.h \
	page-wimax.c \
	page-infiniband.h \
	page-infiniband.c \
	page-ip4.h \
	page-ip4.c \
	page-ip6.h \
	page-ip6.c \
	page-dsl.h \
	page-dsl.c \
	page-mobile.h \
	page-mobile.c \
	page-bluetooth.h \
	page-bluetooth.c \
	page-ppp.h \
	page-ppp.c \
	page-vpn.h \
	page-vpn.c \
	page-master.h \
	page-master.c \
	page-bond.h \
	page-bond.c \
	page-team.h \
	page-team.c \
	page-team-port.h \
	page-team-port.c \
	page-bridge.h \
	page-bridge.c \
	page-bridge-port.h \
	page-bridge-port.c \
	page-vlan.h \
	page-vlan.c \
	page-dcb.c \
	page-dcb.h \
	vpn-helpers.h \
	vpn-helpers.c \
	ip4-routes-dialog.h \
	ip4-routes-dialog.c \
	ip6-routes-dialog.h \
	ip6-routes-dialog.c \
	ppp-auth-methods-dialog.c \
	ppp-auth-methods-dialog.h \
	ce-polkit-button.c \
	ce-polkit-button.h \
	connection-helpers.c \
	connection-helpers.h

nm-connection-editor-service-glue.h: $(top_srcdir)/src/connection-editor/nm-connection-editor-service.xml
	$(AM_V_GEN) dbus-binding-tool --prefix=nm_connection_editor_service --mode=glib-server --output=$@ $<

nm_connection_editor_LDADD = \
	${top_builddir}/src/wireless-security/libwireless-security.la \
	${top_builddir}/src/utils/libutils.la \
	${top_builddir}/src/marshallers/libmarshallers.la \
	${top_builddir}/src/libnm-gtk/libnm-gtk.la \
	$(GTK_LIBS) \
	$(NMA_LIBS) \
	-lm

uidir = $(datadir)/nm-applet
ui_DATA = \
	nm-connection-editor.ui \
	ce-new-connection.ui \
	ce-page-general.ui \
	ce-page-ethernet.ui \
	ce-page-wifi.ui \
	ce-page-wifi-security.ui \
	ce-page-wimax.ui \
	ce-page-infiniband.ui \
	ce-page-bond.ui \
	ce-page-team.ui \
	ce-page-team-port.ui \
	ce-page-bridge.ui \
	ce-page-bridge-port.ui \
	ce-page-ip4.ui \
	ce-ip4-routes.ui \
	ce-page-ip6.ui \
	ce-ip6-routes.ui \
	ce-page-dsl.ui \
	ce-page-mobile.ui \
	ce-page-bluetooth.ui \
	ce-page-ppp.ui \
	ce-ppp-auth-methods.ui \
	ce-page-vlan.ui \
	ce-page-dcb.ui

BUILT_SOURCES = nm-connection-editor-service-glue.h

CLEANFILES = *.bak $(BUILT_SOURCES)

EXTRA_DIST = $(ui_DATA) nm-connection-editor-service.xml

