include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgeocode-glib.la

CACHE_DIR = "/var/cache/geoclue-2.0"

AM_CPPFLAGS = $(GEOCLUE_CFLAGS) 		     	  \
	      $(WARN_CFLAGS)			     	  \
	      -DGEOCODE_LOCALEDIR="\"$(datadir)/locale\"" \
	      -DG_LOG_DOMAIN=\""Geoclue"\"	     	  \
	      -DABS_TOP_SRCDIR=\""$(abs_top_srcdir)"\" 	  \
	      -DSYSCONFDIR=\""$(sysconfdir)"\"		  \
	      -DPACKAGE_VERSION=\"@PACKAGE_VERSION@\"     \
	      -I$(top_srcdir)/src			  \
	      -I$(top_builddir)/src

libgeocode_glib_la_SOURCES = 				\
	geocode-location.h				\
	geocode-location.c				\
	geocode-forward.h				\
	geocode-forward.c				\
	geocode-reverse.h				\
	geocode-reverse.c				\
	geocode-glib.h					\
	geocode-glib.c					\
	geocode-error.h                                 \
	geocode-error.c                                 \
	geocode-enum-types.h				\
	geocode-enum-types.c				\
	geocode-place.h					\
	geocode-place.c					\
	geocode-bounding-box.h				\
	geocode-bounding-box.c				\
	geocode-glib-private.h				\
	$(NULL)
libgeocode_glib_la_LIBADD = $(GEOCLUE_LIBS) $(LIBS)

GEOCODEGLIBDIR=$(top_srcdir)/../geocode-glib/geocode-glib/
GEOCODEGLIBFILES=$(libgeocode_glib_la_SOURCES)
update-from-geocode-glib:
	FILES="$(GEOCODEGLIBFILES)" DIR="$(GEOCODEGLIBDIR)" $(srcdir)/update-from-geocode-glib.sh && changed=true ; \
	git commit -m "location: Update from geocode-glib" $(GEOCODEGLIBFILES)

-include $(top_srcdir)/git.mk
