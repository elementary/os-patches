include $(GLIB_MAKEFILE)

noinst_LTLIBRARIES = libmarshallers.la

GLIB_GENERATED = \
	nma-marshal.h \
	nma-marshal.c

BUILT_SOURCES = $(GLIB_GENERATED)

nma_marshal_sources = \
	$(top_srcdir)/src/*.c \
	$(top_srcdir)/src/gnome-bluetooth/*.c \
	$(top_srcdir)/src/connection-editor/*.c

libmarshallers_la_SOURCES = \
	nma-marshal.h \
	nma-marshal.c

libmarshallers_la_CPPFLAGS = \
	$(GOBJECT_CFLAGS) \
	-DG_DISABLE_DEPRECATED


libmarshallers_la_LIBADD = $(GOBJECT_LIBS)

