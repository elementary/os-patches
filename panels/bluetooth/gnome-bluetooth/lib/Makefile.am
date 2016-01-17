EXTRA_DIST = 
CLEANFILES =

lib_LTLIBRARIES = libgnome-bluetooth.la

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
	bluetooth-fdo-glue.c

libgnome_bluetooth_private_headers =	\
	bluetooth-client-private.h	\
	bluetooth-agent.h		\
	gnome-bluetooth-enum-types.h	\
	bluetooth-chooser-private.h	\
	bluetooth-client-glue.h		\
	bluetooth-fdo-glue.h

# public headers don't need to be listed, are handled by _HEADERS
libgnome_bluetooth_la_SOURCES =			\
	$(libgnome_bluetooth_private_headers)	\
	$(libgnome_bluetooth_c_sources)

libgnome_bluetooth_la_LIBADD = $(BLUETOOTH_PANEL_LIBS)

libgnome_bluetooth_introspect_headers =			\
	bluetooth-client.h				\
	bluetooth-chooser.h				\
	bluetooth-chooser-button.h			\
	bluetooth-chooser-combo.h			\
	bluetooth-filter-widget.h			\
	bluetooth-enums.h				\
	bluetooth-utils.h

gnomebluetoothdir = $(pkgincludedir)
gnomebluetooth_HEADERS = $(libgnome_bluetooth_introspect_headers)

AM_CFLAGS =						\
	-I$(srcdir)					\
	$(BLUETOOTH_PANEL_CFLAGS)			\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	-DG_LOG_DOMAIN=\"Bluetooth\"

BUILT_SOURCES = bluetooth-client-glue.h \
		bluetooth-client-glue.c \
		bluetooth-fdo-glue.h \
		bluetooth-fdo-glue.c \
		gnome-bluetooth-enum-types.h \
		gnome-bluetooth-enum-types.c

CLEANFILES += $(BUILT_SOURCES)

EXTRA_DIST += bluetooth-client.xml bluetooth-fdo.xml

MAINTAINERCLEANFILES = Makefile.in

bluetooth-client-glue.c: bluetooth-client-glue.h

bluetooth-client-glue.c bluetooth-client-glue.h: bluetooth-client.xml
	$(AM_V_GEN) gdbus-codegen --c-namespace= --generate-c-code=bluetooth-client-glue --interface-prefix=org.bluez $<

bluetooth-fdo-glue.c: bluetooth-fdo-glue.h

bluetooth-fdo-glue.c bluetooth-fdo-glue.h: bluetooth-fdo.xml
	$(AM_V_GEN) gdbus-codegen --c-namespace= --generate-c-code=bluetooth-fdo-glue --interface-prefix=org.freedesktop.DBus $<

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
