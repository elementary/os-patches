DOC_MODULE = messaging-menu

DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

DOC_SOURCE_DIR = $(top_srcdir)/libmessaging-menu

MKDB_OPTIONS=--xml-mode --output-format=xml

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB = $(top_srcdir)/libmessaging-menu/*.h
CFILE_GLOB = $(top_srcdir)/libmessaging-menu/*.c

IGNORE_HFILES= \
	indicator-messages-service.h

INCLUDES=-I$(top_srcdir)/libmessaging-menu $(GIO_CFLAGS)
GTKDOC_LIBS=$(top_builddir)/libmessaging-menu/libmessaging-menu.la

include $(top_srcdir)/gtk-doc.make
