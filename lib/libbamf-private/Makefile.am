include Makefile.am.notparallel

noinst_LTLIBRARIES = libbamf-private.la

bamf_gdbus_files = \
	bamf-gdbus-generated.c \
	bamf-gdbus-generated.h \
	bamf-gdbus-view-generated.c \
	bamf-gdbus-view-generated.h \
	$(NULL)

bamf_xml_gdbus_files = \
	org.ayatana.bamf.xml \
	org.ayatana.bamf.view.xml \
	$(NULL)

bamf_built_sources = \
	$(bamf_gdbus_files) \
	$(NULL)

libbamf_private_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(NULL)

libbamf_private_la_LDFLAGS = \
	-no-undefined \
	$(NULL)

libbamf_private_la_LIBADD = \
	$(GLIB_LIBS) \
	$(NULL)

libbamf_private_la_SOURCES = \
	bamf-private.h \
	$(NULL)

nodist_libbamf_private_la_SOURCES = \
	$(bamf_built_sources) \
	$(NULL)

CLEANFILES = \
	$(bamf_built_sources) \
	$(NULL)

DISTCLEANFILES = \
	$(bamf_built_sources) \
	$(NULL)

BUILT_SOURCES = \
	$(bamf_gdbus_files) \
	$(NULL)

EXTRA_DIST = \
	$(bamf_xml_gdbus_files) \
	$(NULL)
