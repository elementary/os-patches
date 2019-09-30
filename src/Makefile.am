
EXTRA_DIST =

pkglibexec_PROGRAMS = indicator-messages-service

indicator_messages_service_SOURCES = \
	messages-service.c \
	dbus-data.h \
	gactionmuxer.c \
	gactionmuxer.h \
	gsettingsstrv.c \
	gsettingsstrv.h \
	im-menu.c \
	im-menu.h \
	im-phone-menu.c \
	im-phone-menu.h \
	im-desktop-menu.c \
	im-desktop-menu.h \
	im-application-list.c \
	im-application-list.h \
	indicator-desktop-shortcuts.c \
	indicator-desktop-shortcuts.h

indicator_messages_service_CFLAGS = \
	$(APPLET_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-I$(top_builddir)/common \
	-Wall \
	-Wl,-Bsymbolic-functions \
	-Wl,-z,defs \
	-Wl,--as-needed \
	-Werror -Wno-error=deprecated-declarations \
	-DG_LOG_DOMAIN=\"Indicator-Messages\"

indicator_messages_service_LDADD = \
	$(top_builddir)/common/libmessaging-common.la \
	$(APPLET_LIBS)

indicator_messages_service_LDFLAGS = \
	$(COVERAGE_LDFLAGS)

EXTRA_DIST += \
	messages-service.xml
