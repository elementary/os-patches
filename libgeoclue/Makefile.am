include $(top_srcdir)/Makefile.decl

CLIENT_INTERFACE_XML = $(top_srcdir)/src/org.freedesktop.GeoClue2.Client.xml
LOCATION_INTERFACE_XML = $(top_srcdir)/src/org.freedesktop.GeoClue2.Location.xml
MANAGER_INTERFACE_XML = $(top_srcdir)/src/org.freedesktop.GeoClue2.Manager.xml

dbus_built_sources = gclue-client.c   \
		     gclue-client.h   \
		     gclue-location.c \
		     gclue-location.h \
		     gclue-manager.c  \
		     gclue-manager.h

gclue-client.c: gclue-client.h
gclue-client.h: Makefile.am $(CLIENT_INTERFACE_XML)
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.GeoClue2.	\
		--c-namespace GClue				\
		--generate-c-code gclue-client			\
		$(CLIENT_INTERFACE_XML)

gclue-location.c: gclue-location.h
gclue-location.h: Makefile.am $(LOCATION_INTERFACE_XML)
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.GeoClue2.	\
		--c-namespace GClue				\
		--generate-c-code gclue-location		\
		$(LOCATION_INTERFACE_XML)

gclue-manager.c: gclue-manager.h
gclue-manager.h: Makefile.am $(MANAGER_INTERFACE_XML)
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.GeoClue2.	\
		--c-namespace GClue				\
		--generate-c-code gclue-manager			\
		$(MANAGER_INTERFACE_XML)

BUILT_SOURCES = 		\
	$(dbus_built_sources)	\
	$(NULL)

lib_LTLIBRARIES = libgeoclue-2.la

geoclue_includedir = $(includedir)/libgeoclue-$(GEOCLUE_API_VERSION)
geoclue_include_HEADERS = geoclue.h          \
			  gclue-client.h     \
			  gclue-location.h   \
			  gclue-manager.h    \
			  gclue-helpers.h    \
			  gclue-simple.h     \
			  $(NULL)

AM_CPPFLAGS = $(LIBGEOCLUE_CFLAGS) 		     	  \
	      $(WARN_CFLAGS)			     	  \
	      -DLOCALEDIR="\"$(datadir)/locale\"" 	  \
	      -DG_LOG_DOMAIN=\""Geoclue"\"	     	  \
	      -DABS_TOP_SRCDIR=\""$(abs_top_srcdir)"\" 	  \
	      -DSYSCONFDIR=\""$(sysconfdir)"\"		  \
	      -I$(top_srcdir)/public-api		  \
	      -I$(top_builddir)/public-api

ENUMS_HEADER = $(top_srcdir)/public-api/gclue-enums.h
ENUMS_LIB = $(top_builddir)/public-api/libgeoclue-public-api.la

libgeoclue_2_la_SOURCES =  \
	gclue-client.c 	   \
	gclue-location.c   \
	gclue-manager.c    \
	gclue-helpers.c    \
	gclue-simple.c     \
	$(NULL)

libgeoclue_2_la_LIBADD =   \
	$(LIBGEOCLUE_LIBS) \
	$(LIBS)		   \
	$(ENUMS_LIB) 	   \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES) \
	     $(NULL)

if HAVE_INTROSPECTION
girdir = $(datadir)/gir-1.0
gir_DATA = Geoclue-2.0.gir

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = Geoclue-2.0.typelib

Geoclue-2.0.gir: $(libgeoclue_2_la_SOURCES) $(geoclue_include_HEADERS)
	$(AM_V_GEN) env CPPFLAGS="$(CPPFLAGS)"  \
	    CFLAGS="$(CFLAGS)"			\
	    LDFLAGS="$(LDFLAGS)"		\
	    CC="$(CC)"				\
	    PKG_CONFIG="$(PKG_CONFIG)"		\
	    DLLTOOL="$(DLLTOOL)" 		\
	    $(INTROSPECTION_SCANNER) -v 	\
	    --warn-all                          \
	    --namespace Geoclue                 \
	    --identifier-prefix=GClue           \
	    --symbol-prefix=gclue               \
	    --nsversion=2.0                     \
	    --include=Gio-2.0                   \
	    --library=libgeoclue-2.la	        \
	    --output $@                         \
	    --pkg=glib-2.0                      \
	    --pkg=gobject-2.0                   \
	    --pkg=gio-2.0                       \
	    --pkg-export=libgeoclue-2.0         \
	    --libtool="${LIBTOOL}"   		\
	    -I$(srcdir)				\
	    -I$(top_srcdir)/public-api		\
	    -I$(top_builddir)/public-api	\
	    --c-include='geoclue.h'             \
	    $^                                  \
	    $(ENUMS_HEADER) 			\
	    $(NULL)

Geoclue-2.0.gir: libgeoclue-2.la $(INTROSPECTION_SCANNER) Makefile.am
Geoclue-2.0.typelib: Geoclue-2.0.gir $(INTROSPECTION_COMPILER)
	$(INTROSPECTION_COMPILER) $< -o $@

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif # HAVE_INTROSPECTION

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgeoclue-$(GEOCLUE_API_VERSION).pc

DISTCLEANFILES = $(pkgconfig_DATA)

-include $(top_srcdir)/git.mk
