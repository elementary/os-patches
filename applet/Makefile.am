CLEANFILES =
EXTRA_DIST =

pkglib_LTLIBRARIES = libgnome-bluetooth-applet.la

libgnome_bluetooth_applet_la_SOURCES = 	\
	bluetooth-applet.c		\
	bluetooth-applet.h

libgnome_bluetooth_applet_la_LIBADD = $(LIBGNOMEBT_LIBS) $(top_builddir)/lib/libgnome-bluetooth.la

libgnome_bluetooth_applet_la_LDFLAGS = -no-undefined

AM_CFLAGS = -I$(srcdir) -I$(top_srcdir)/lib $(LIBGNOMEBT_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED)

install-exec-hook:
	rm -f $(DESTDIR)$(pkglibdir)/libgnome-bluetooth-applet.la
uninstall-hook:
	rm -f $(DESTDIR)$(pkglibdir)/libgnome-bluetooth-applet.so*

include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir) --includedir=$(top_builddir)/lib

if HAVE_INTROSPECTION
GnomeBluetoothApplet-1.0.gir: libgnome-bluetooth-applet.la
GnomeBluetoothApplet_1_0_gir_SCANNERFLAGS = --warn-all --symbol-prefix=bluetooth --identifier-prefix=Bluetooth --include-uninstalled=$(top_builddir)/lib/GnomeBluetooth-1.0.gir
GnomeBluetoothApplet_1_0_gir_INCLUDES = GObject-2.0 Gtk-3.0 GModule-2.0
GnomeBluetoothApplet_1_0_gir_PACKAGES = gobject-2.0 gtk+-3.0 gmodule-2.0 glib-2.0
GnomeBluetoothApplet_1_0_gir_CFLAGS = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/lib
GnomeBluetoothApplet_1_0_gir_LIBS = libgnome-bluetooth-applet.la
GnomeBluetoothApplet_1_0_gir_FILES = $(libgnome_bluetooth_applet_la_SOURCES) ../lib/bluetooth-enums.h
INTROSPECTION_GIRS += GnomeBluetoothApplet-1.0.gir

typelibdir = $(pkglibdir)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(typelib_DATA) $(INTROSPECTION_GIRS)
endif # HAVE_INTROSPECTION
