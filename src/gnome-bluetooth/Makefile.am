AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DICONDIR=\"$(icondir)\" \
	-DLOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_builddir) \
	-I${top_builddir}/src/marshallers \
	-I${top_srcdir}/src/utils \
	-I${top_srcdir}/src/libnm-gtk \
	$(GNOME_BLUETOOTH_CFLAGS) \
	$(DISABLE_DEPRECATED) \
	$(WARN_CFLAGS)

BT_WIDGET_SOURCES = \
	bt-widget.c \
	nma-bt-device.c \
	nma-bt-device.h

if HAVE_GBT
plugindir = $(libdir)/gnome-bluetooth/plugins

plugin_LTLIBRARIES = libnma.la

libnma_la_SOURCES = $(BT_WIDGET_SOURCES)

libnma_la_CPPFLAGS = $(AM_CPPFLAGS)

if WITH_MODEM_MANAGER_1
libnma_la_CPPFLAGS += $(MM_GLIB_CFLAGS)
endif

libnma_la_LDFLAGS = -module -avoid-version

libnma_la_LIBADD = \
	$(top_builddir)/src/marshallers/libmarshallers.la \
	$(top_builddir)/src/utils/libutils.la \
	$(top_builddir)/src/libnm-gtk/libnm-gtk.la \
	$(GNOME_BLUETOOTH_LIBS)

if WITH_MODEM_MANAGER_1
libnma_la_LIBADD += $(MM_GLIB_LIBS)
endif

endif

EXTRA_DIST = $(BT_WIDGET_SOURCES)
