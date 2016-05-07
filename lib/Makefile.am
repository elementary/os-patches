EXTRA_DIST = 
CLEANFILES =
BUILT_SOURCES =

lib_LTLIBRARIES = libgnome-bluetooth.la

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/bluetooth.gresource.xml)
bluetooth-settings-resources.c: bluetooth.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name bluetooth_settings $<
bluetooth-settings-resources.h: bluetooth.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name bluetooth_settings $<

EXTRA_DIST += bluetooth.gresource.xml settings.ui bluetooth-pairing-dialog.ui bluetooth-settings-row.ui bluetooth-settings.css
BUILT_SOURCES += bluetooth-settings-resources.c bluetooth-settings-resources.h

# will be scanned for introspection annotation, but won't be installed
libgnome_bluetooth_c_sources =		\
	bluetooth-client.c		\
	bluetooth-utils.c		\
	bluetooth-agent.c		\
	gnome-bluetooth-enum-types.c	\
	bluetooth-chooser.c		\
	bluetooth-filter-widget.c	\
	bluetooth-chooser-button.c	\
	bluetooth-chooser-combo.c	\
	bluetooth-client-glue.c		\
	bluetooth-fdo-glue.c		\
	bluetooth-settings-widget.c	\
	bluetooth-settings-resources.c	\
	bluetooth-settings-row.c	\
	bluetooth-pairing-dialog.c	\
	pin.c				\
	bluetooth-settings-obexpush.c

libgnome_bluetooth_private_headers =	\
	bluetooth-client-private.h	\
	bluetooth-agent.h		\
	gnome-bluetooth-enum-types.h	\
	bluetooth-chooser-private.h	\
	bluetooth-client-glue.h		\
	bluetooth-fdo-glue.h		\
	bluetooth-settings-resources.h	\
	bluetooth-settings-row.h	\
	bluetooth-pairing-dialog.h	\
	pin.h				\
	bluetooth-settings-obexpush.h

# public headers don't need to be listed, are handled by _HEADERS
libgnome_bluetooth_la_SOURCES =			\
	$(libgnome_bluetooth_private_headers)	\
	$(libgnome_bluetooth_c_sources)

libgnome_bluetooth_la_LIBADD = $(LIBGNOMEBT_LIBS) $(LIBS)

libgnome_bluetooth_la_LDFLAGS =				\
	-version-info $(GNOMEBT_LT_VERSION)		\
	-no-undefined					\
	$(AM_LDFLAGS)					\
	-export-symbols $(srcdir)/gnome-bluetooth.symbols

libgnome_bluetooth_introspect_headers =			\
	bluetooth-client.h				\
	bluetooth-chooser.h				\
	bluetooth-chooser-button.h			\
	bluetooth-chooser-combo.h			\
	bluetooth-filter-widget.h			\
	bluetooth-enums.h				\
	bluetooth-utils.h				\
	bluetooth-settings-widget.h

gnomebluetoothdir = $(pkgincludedir)
gnomebluetooth_HEADERS = $(libgnome_bluetooth_introspect_headers)

AM_CFLAGS =						\
	-I$(srcdir)					\
	$(LIBGNOMEBT_CFLAGS)				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	-DPKGDATADIR="\"$(pkgdatadir)\""		\
	-DG_LOG_DOMAIN=\"Bluetooth\"

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_files =				 \
	$(libgnome_bluetooth_introspect_headers) \
	$(libgnome_bluetooth_c_sources)

GnomeBluetooth-1.0.gir: libgnome-bluetooth.la
GnomeBluetooth_1_0_gir_INCLUDES = GObject-2.0 Gtk-3.0 GModule-2.0
GnomeBluetooth_1_0_gir_PACKAGES = gobject-2.0 gtk+-3.0 gmodule-2.0 glib-2.0
GnomeBluetooth_1_0_gir_CFLAGS = -I$(srcdir)
GnomeBluetooth_1_0_gir_LIBS = libgnome-bluetooth.la
GnomeBluetooth_1_0_gir_SCANNERFLAGS = --symbol-prefix=bluetooth --identifier-prefix=Bluetooth --pkg-export=gnome-bluetooth-1.0
GnomeBluetooth_1_0_gir_FILES = $(introspection_files)
INTROSPECTION_GIRS += GnomeBluetooth-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

endif # HAVE_INTROSPECTION

BUILT_SOURCES += bluetooth-client-glue.h \
		 bluetooth-client-glue.c \
		 bluetooth-fdo-glue.h \
		 bluetooth-fdo-glue.c \
		 gnome-bluetooth-enum-types.h \
		 gnome-bluetooth-enum-types.c

CLEANFILES += $(BUILT_SOURCES)

noinst_PROGRAMS = test-client test-agent test-deviceselection test-class test-settings test-pairing-dialog test-pin

test_client_LDADD = libgnome-bluetooth.la $(LIBGNOMEBT_LIBS)

test_agent_LDADD = libgnome-bluetooth.la $(LIBGNOMEBT_LIBS)

test_deviceselection_LDADD = libgnome-bluetooth.la $(LIBGNOMEBT_LIBS)

test_class_LDADD = libgnome-bluetooth.la $(LIBGNOMEBT_LIBS)

test_settings_LDADD = libgnome-bluetooth.la $(LIBGNOMEBT_LIBS)

test_pairing_dialog_LDADD = libgnome-bluetooth.la $(LIBGNOMEBT_LIBS)

test_pin_SOURCES = test-pin.c
test_pin_LDADD = libgnome-bluetooth.la $(LIBGNOMEBT_LIBS)

pin_DATA = pin-code-database.xml
pindir = $(pkgdatadir)

pin-code-database.xml.valid: pin-code-database.xml
	@if test -n $(XMLLINT) ; then \
		xmllint --noout --valid $(srcdir)/pin-code-database.xml ;	\
	fi
	touch $@

CLEANFILES += pin-code-database.xml.valid

all-local: pin-code-database.xml.valid

EXTRA_DIST += bluetooth-client.xml bluetooth-fdo.xml gnome-bluetooth.symbols $(pin_DATA)

MAINTAINERCLEANFILES = Makefile.in

bluetooth-client-glue.c: bluetooth-client-glue.h

bluetooth-client-glue.c bluetooth-client-glue.h: bluetooth-client.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace= --generate-c-code=bluetooth-client-glue --interface-prefix=org.bluez $<

bluetooth-fdo-glue.c: bluetooth-fdo-glue.h

bluetooth-fdo-glue.c bluetooth-fdo-glue.h: bluetooth-fdo.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace= --generate-c-code=bluetooth-fdo-glue --interface-prefix=org.freedesktop.DBus $<

gnome_bluetooth_headers_to_scan_for_enums = bluetooth-enums.h
# Generate the enums source code, with glib-mkenums:
# This is based on the same Makefile.am stuff in pango:
gnome_bluetooth_built_headers = gnome-bluetooth-enum-types.h
gnome_bluetooth_built_cfiles = gnome-bluetooth-enum-types.c

gnome-bluetooth-enum-types.h: $(gnome_bluetooth_headers_to_scan_for_enums) Makefile
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef __GNOME_BLUETOOTH_ENUM_TYPES_H__\n#define __GNOME_BLUETOOTH_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define BLUETOOTH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __GNOME_BLUETOOTH_ENUM_TYPES_H__ */" \
		$(gnome_bluetooth_headers_to_scan_for_enums)) > $@

gnome-bluetooth-enum-types.c: $(gnome_bluetooth_headers_to_scan_for_enums) Makefile gnome-bluetooth-enum-types.h
	$(AM_V_GEN) (cd $(srcdir) && glib-mkenums \
			--fhead "#include <bluetooth-enums.h>\n" \
			--fhead "#include \"gnome-bluetooth-enum-types.h\"\n" \
			--fhead "#include <glib-object.h>" \
		      	--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(gnome_bluetooth_headers_to_scan_for_enums)) > $@
