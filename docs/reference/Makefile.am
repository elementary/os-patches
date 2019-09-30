## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=libaccounts-glib

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR=$(top_srcdir)/libaccounts-glib

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS=--rebuild-types --deprecated-guards="AG_DISABLE_DEPRECATED"

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS=--source-dir=$(top_srcdir)/libaccounts-glib --sgml-mode --output-format=xml --name-space=ag

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=$(top_srcdir)/libaccounts-glib/*.h
CFILE_GLOB=$(top_srcdir)/libaccounts-glib/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=ag-marshal.h ag-debug.h ag-internals.h ag-util.h

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files= \
	ag-backup.xml \
	ag-tool.xml \
	application-file-format.xml \
	gettext-xml-files.xml \
	libaccounts-compiling.xml \
	libaccounts-glossary.xml \
	libaccounts-overview.xml \
	libaccounts-running.xml \
	provider-file-format.xml \
	service-file-format.xml \
	service-type-file-format.xml \
	validating-xml-files.xml

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
AM_CPPFLAGS=-I$(top_srcdir) $(LIBACCOUNTS_CFLAGS)
GTKDOC_LIBS=$(top_builddir)/libaccounts-glib/libaccounts-glib.la $(LIBACCOUNTS_LIBS)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += 

if ENABLE_MAN
ag-backup.1: ag-backup.xml
	$(AM_V_GEN)xsltproc -nonet --path $(builddir) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

ag-tool.1: ag-tool.xml
	$(AM_V_GEN)xsltproc -nonet --path $(builddir) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

nodist_man_MANS = \
	ag-backup.1 \
	ag-tool.1

# Already defined in gtk-doc.make.
CLEANFILES += $(nodist_man_MANS)
endif

if ENABLE_GTK_DOC
# Disabled until https://bugzilla.gnome.org/show_bug.cgi?id=701638 is fixed.
#TESTS = $(GTKDOC_CHECK)
endif
