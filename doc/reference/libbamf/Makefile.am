## Process this file with automake to produce Makefile.in

# automake requirements
AUTOMAKE_OPTIONS = 1.7

# The name of the module
DOC_MODULE=libbamf

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
DOC_SOURCE_DIR = ../../../lib/libbamf

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS =

# Extra options to supply to gtkdoc-scan.
# Fx --rebuild-types --rebuild-sections
SCAN_OPTIONS =

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS = --sgml-mode --output-format=xml --ignore-files=trio

# Extra options to supply to gtkdoc-fixref. Not normally needed.
FIXXREF_OPTIONS =

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB = $(top_srcdir)/lib/libbamf/bamf-*.h
CFILE_GLOB = $(top_srcdir)/lib/libbamf/bamf-*.c

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
INCLUDES= \
  -I$(top_srcdir)/lib/libbamf   \
  $(GTK_CFLAGS) \
  $(GLIB_CFLAGS) \
  $(DBUS_CFLAGS)

GTKDOC_LIBS = \
  $(GTK_LIBS) \
  $(GLIB_LIBS) \
  $(top_builddir)/lib/libbamf/libbamf$(LIBBAMF_VER).la

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST +=
