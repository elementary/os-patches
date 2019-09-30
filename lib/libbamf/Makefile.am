-include $(INTROSPECTION_MAKEFILE)

LIBBAMF_PRIVATE_LIB = $(top_builddir)/lib/libbamf-private/libbamf-private.la

libbamf_sources = \
	bamf-application.c \
	bamf-control.c \
	bamf-matcher.c \
	bamf-view.c \
	bamf-window.c \
	bamf-factory.c \
	bamf-tab.c \
	$(NULL)

libbamf_headers = \
	bamf-application.h \
	bamf-control.h \
	bamf-matcher.h \
	bamf-view.h \
	bamf-window.h \
	bamf-tab.h \
	libbamf.h \
	$(NULL)

libbamf_private_headers = \
	bamf-factory.h \
	bamf-application-private.h \
	bamf-view-private.h \
	$(NULL)

libbamf_la_SOURCES = \
	$(libbamf_built_sources) \
	$(libbamf_private_headers) \
	$(libbamf_headers) \
	$(libbamf_sources) \
	$(NULL)

libbamf_la_LIBADD = \
	$(LIBBAMF_PRIVATE_LIB) \
	$(LIBWSBIND_LIBS) \
	$(GLIB_LIBS) \
	$(COVERAGE_LIBS) \
	$(NULL)

libbamf_la_CFLAGS = \
	-Wall -std=c99 \
	-fPIC \
	-DDATADIR=\""$(datadir)"\" \
	-I$(srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	$(COVERAGE_CFLAGS) \
	$(LIBWSBIND_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(NULL)

libbamf_la_LDFLAGS = \
	-shared \
	-export-symbols-regex "^bamf_" \
	-version-info 2:0:0 \
	$(COVERAGE_LDFLAGS) \
	$(NULL)

lib_LTLIBRARIES = libbamf3.la
libbamf3_la_SOURCES = $(libbamf_la_SOURCES)
libbamf3_la_LIBADD = $(libbamf_la_LIBADD)
libbamf3_la_CFLAGS = $(libbamf_la_CFLAGS)
libbamf3_la_LDFLAGS = $(libbamf_la_LDFLAGS)

libbamf_includedir=$(includedir)/libbamf$(LIBBAMF_VER)/libbamf
libbamf_include_HEADERS = \
	$(libbamf_headers)

libbamf_pcdir = $(libdir)/pkgconfig
libbamf_pc_DATA = \
	libbamf$(LIBBAMF_VER).pc

BUILT_SOURCES = \
	$(libbamf_gdbus_files) \
	$(NULL)

DISTCLEANFILES = \
	$(libbamf_built_sources) \
	$(NULL)

CLEANFILES = \
	$(libbamf_built_sources) \
	$(NULL)


#########################
# GObject Introsepction
#########################

if HAVE_INTROSPECTION

INTROSPECTION_GIRS = Bamf-3.gir
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

introspection_sources = \
	$(libbamf_headers) \
	$(libbamf_sources) \
	$(NULL)

Bamf-3.gir: libbamf$(LIBBAMF_VER).la
Bamf_3_gir_INCLUDES = GObject-2.0 GLib-2.0
Bamf_3_gir_SCANNERFLAGS = --warn-all --c-include='libbamf/libbamf.h'
Bamf_3_gir_CFLAGS = \
	$(DBUS_CFLAGS) \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	$(NULL)
Bamf_3_gir_LIBS = libbamf$(LIBBAMF_VER).la
Bamf_3_gir_FILES = $(introspection_sources)
Bamf_3_gir_EXPORT_PACKAGES = libbamf$(LIBBAMF_VER)

girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(INTROSPECTION_TYPELIBDIR)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

endif

#########################
# VAPI Files
#########################

if HAVE_INTROSPECTION

vapidir = $(datadir)/vala/vapi
vapi_DATA = libbamf$(LIBBAMF_VER).vapi

$(vapi_DATA): $(INTROSPECTION_GIRS)
	$(AM_V_GEN)$(VALA_API_GEN) --library=$(@:.vapi=) $<

CLEANFILES += $(vapi_DATA)

endif

