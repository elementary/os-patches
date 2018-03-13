include $(top_srcdir)/Makefile.decl

BUILT_SOURCES = 		\
	gclue-enum-types.h	\
	gclue-enum-types.c	\
	$(NULL)

geoclue_includedir = $(includedir)/libgeoclue-$(GEOCLUE_API_VERSION)
geoclue_include_HEADERS = gclue-enums.h	     \
			  gclue-enum-types.h \
			  $(NULL)

AM_CPPFLAGS = $(GEOCLUE_CFLAGS) 		       \
	      $(WARN_CFLAGS)			       \
	      -DLOCALEDIR="\"$(datadir)/locale\""      \
	      -DG_LOG_DOMAIN=\""Geoclue"\"	       \
	      -DABS_TOP_SRCDIR=\""$(abs_top_srcdir)"\" \
	      -DSYSCONFDIR=\""$(sysconfdir)"\"

noinst_LTLIBRARIES = libgeoclue-public-api.la
libgeoclue_public_api_la_SOURCES = $(BUILT_SOURCES)

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = gclue-enums.h

# Enum types
GEOCLUE_ENUMS = \
	$(srcdir)/gclue-enums.h

gclue-enum-types.h: Makefile.am $(GEOCLUE_ENUMS) $(top_srcdir)/build-aux/gclue-enums-template.h
	$(AM_V_GEN) $(GLIB_MKENUMS) \
	    --fhead "#include \"gclue-enums.h\"\n#ifndef __GCLUE_ENUM_TYPES_H__\n#define __GCLUE_ENUM_TYPES_H__\n" \
	    --template $(top_srcdir)/build-aux/gclue-enums-template.h \
	    --ftail "#endif /* __GCLUE_ENUM_TYPES_H__ */\n" \
	    $(GEOCLUE_ENUMS) > $@

gclue-enum-types.c: Makefile.am $(top_srcdir)/build-aux/gclue-enums-template.c gclue-enum-types.h
	$(AM_V_GEN) $(GLIB_MKENUMS) \
	    --fhead "#include \"gclue-enum-types.h\"" \
	    --template $(top_srcdir)/build-aux/gclue-enums-template.c \
	    $(GEOCLUE_ENUMS) > $@

-include $(top_srcdir)/git.mk
