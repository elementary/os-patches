include $(top_srcdir)/Makefile.decl

libexec_PROGRAMS = geoclue

SUBDIRS = agent geocode-glib

CLIENT_INTERFACE_XML = org.freedesktop.GeoClue2.Client.xml
LOCATION_INTERFACE_XML = org.freedesktop.GeoClue2.Location.xml
MANAGER_INTERFACE_XML = org.freedesktop.GeoClue2.Manager.xml

interfacedir = $(datadir)/dbus-1/interfaces
interface_DATA = $(CLIENT_INTERFACE_XML)   \
		 $(LOCATION_INTERFACE_XML) \
		 $(MANAGER_INTERFACE_XML)

# Provide a single interface file too for backwards compatiblity.
# At least gnome-settings-daemon currently relies on that.
install-data-hook:
	{ (echo "<node>") && 						     \
	  (tail -n +10 $(DESTDIR)$(interfacedir)/$(MANAGER_INTERFACE_XML) |  \
	   grep -v node) &&  						     \
	  (tail -n +10 $(DESTDIR)$(interfacedir)/$(CLIENT_INTERFACE_XML) |   \
	   grep -v node) &&   						     \
	  (tail -n +10 $(DESTDIR)$(interfacedir)/$(LOCATION_INTERFACE_XML) | \
	   grep -v node) && 						     \
	  (echo "</node>") } 						     \
	> $(DESTDIR)$(interfacedir)/org.freedesktop.GeoClue2.xml
uninstall-hook:
	rm -r $(DESTDIR)$(interfacedir)/org.freedesktop.GeoClue2.xml

dbus_built_sources = gclue-client-interface.c 	\
		     gclue-client-interface.h 	\
		     gclue-location-interface.c \
		     gclue-location-interface.h \
		     gclue-manager-interface.c 	\
		     gclue-manager-interface.h 	\
		     compass-interface.c 	\
		     compass-interface.h	\
		     wpa_supplicant-interface.c \
		     wpa_supplicant-interface.h

gclue-client-interface.c: gclue-client-interface.h
gclue-client-interface.h: Makefile.am $(CLIENT_INTERFACE_XML)
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.GeoClue2.	\
		--c-namespace GClueDBus				\
		--generate-c-code gclue-client-interface	\
		--generate-docbook=docs				\
		$(srcdir)/$(CLIENT_INTERFACE_XML)

gclue-location-interface.c: gclue-location-interface.h
gclue-location-interface.h: Makefile.am $(LOCATION_INTERFACE_XML)
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.GeoClue2.	\
		--c-namespace GClueDBus				\
		--generate-c-code gclue-location-interface	\
		--generate-docbook=docs				\
		$(srcdir)/$(LOCATION_INTERFACE_XML)

gclue-manager-interface.c: gclue-manager-interface.h
gclue-manager-interface.h: Makefile.am $(MANAGER_INTERFACE_XML)
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.GeoClue2.	\
		--c-namespace GClueDBus				\
		--generate-c-code gclue-manager-interface	\
		--generate-docbook=docs				\
		$(srcdir)/$(MANAGER_INTERFACE_XML)

wpa_supplicant-interface.c: wpa_supplicant-interface.h
wpa_supplicant-interface.h: Makefile.am fi.w1.wpa_supplicant1.xml
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix fi.w1			\
		--annotate "fi.w1.wpa_supplicant1"		\
		org.gtk.GDBus.C.Name WPA_Supplicant		\
		--annotate "fi.w1.wpa_supplicant1.Interface"	\
		org.gtk.GDBus.C.Name WPA_Interface		\
		--annotate "fi.w1.wpa_supplicant1.BSS"		\
		org.gtk.GDBus.C.Name WPA_BSS			\
		--annotate "fi.w1.wpa_supplicant1.BSS:SSID"	\
		org.gtk.GDBus.C.ForceGVariant whatever		\
		--annotate "fi.w1.wpa_supplicant1.BSS:BSSID"	\
		org.gtk.GDBus.C.ForceGVariant whatever		\
		--annotate "fi.w1.wpa_supplicant1.Interface::BSSAdded"	 \
		org.gtk.GDBus.C.Name BSS_Added				 \
		--annotate "fi.w1.wpa_supplicant1.Interface::BSSRemoved" \
		org.gtk.GDBus.C.Name BSS_Removed			 \
		--generate-c-code wpa_supplicant-interface 	\
		$(srcdir)/fi.w1.wpa_supplicant1.xml

compass-interface.c: compass-interface.h
compass-interface.h: Makefile.am net.hadess.SensorProxy.xml
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix net.hadess.SensorProxy \
		--generate-c-code compass-interface 	  \
		$(srcdir)/net.hadess.SensorProxy.xml

gclue-marshal.c: gclue-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=gclue_marshal $(srcdir)/gclue-marshal.list --header --body > gclue-marshal.c

gclue-marshal.h: gclue-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=gclue_marshal $(srcdir)/gclue-marshal.list --header > gclue-marshal.h

BUILT_SOURCES = 		\
	$(dbus_built_sources)	\
	gclue-marshal.c 	\
	gclue-marshal.h		\
	$(NULL)

noinst_LTLIBRARIES = libgeoclue-internal.la

AM_CPPFLAGS = $(GEOCLUE_CFLAGS) 		     	  \
	      $(ModemManager_CFLAGS)			  \
	      $(NMEA_SOURCE_CFLAGS)			  \
	      $(WARN_CFLAGS)			     	  \
	      -DLOCALEDIR="\"$(datadir)/locale\"" 	  \
	      -DG_LOG_DOMAIN=\""Geoclue"\"	     	  \
	      -DABS_TOP_SRCDIR=\""$(abs_top_srcdir)"\" 	  \
	      -DSYSCONFDIR=\""$(sysconfdir)"\"		  \
	      -I$(srcdir)/agent				  \
	      -I$(builddir)/agent			  \
	      -I$(top_srcdir)/public-api		  \
	      -I$(top_builddir)/public-api

libgeoclue_internal_la_SOURCES = \
	$(BUILT_SOURCES) 	 \
	gclue-3g-tower.h 	 \
	gclue-client-info.h 	 \
	gclue-client-info.c 	 \
	gclue-compass.h 	 \
	gclue-compass.c 	 \
	gclue-config.h 	 	 \
	gclue-config.c 	 	 \
	gclue-error.h 	 	 \
	gclue-error.c 	 	 \
	gclue-location-source.h	 \
	gclue-location-source.c	 \
	gclue-locator.h 	 \
	gclue-locator.c 	 \
	gclue-service-manager.h  \
	gclue-service-manager.c  \
	gclue-service-client.h   \
	gclue-service-client.c   \
	gclue-service-location.h \
	gclue-service-location.c \
	gclue-web-source.c	 \
	gclue-web-source.h	 \
	gclue-wifi.h 		 \
	gclue-wifi.c		 \
	gclue-mozilla.h		 \
	gclue-mozilla.c		 \
	gclue-location.h	 \
	gclue-location.c	 \
	$(NULL)

if BUILD_MODEM_SOURCE
libgeoclue_internal_la_SOURCES += gclue-modem.c	        \
				  gclue-modem.h	       \
				  gclue-modem-manager.c \
				  gclue-modem-manager.h
endif

if BUILD_3G_SOURCE
libgeoclue_internal_la_SOURCES += gclue-3g.c gclue-3g.h
endif

if BUILD_CDMA_SOURCE
libgeoclue_internal_la_SOURCES += gclue-cdma.c gclue-cdma.h
endif

if BUILD_MODEM_GPS_SOURCE
libgeoclue_internal_la_SOURCES += gclue-modem-gps.c gclue-modem-gps.h
endif

if BUILD_NMEA_SOURCE
libgeoclue_internal_la_SOURCES += gclue-nmea-source.h gclue-nmea-source.c
endif

libgeoclue_internal_la_LIBADD = 	\
	$(GEOCLUE_LIBS)			\
	$(ModemManager_LIBS)		\
	$(NMEA_SOURCE_LIBS)		\
	$(LIBS)				\
	$(NULL)

geoclue_SOURCES =	 	  \
	gclue-main.c	 	  \
	$(NULL)
geoclue_LDADD = $(GEOCLUE_LIBS) 	  		    	\
		$(LIBS) 		  		    	\
		$(builddir)/libgeoclue-internal.la		    	\
		$(builddir)/agent/libgeoclue-agent.la 		\
		$(builddir)/geocode-glib/libgeocode-glib.la 	\
		$(top_builddir)/public-api/libgeoclue-public-api.la

CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = $(interface_DATA) 	       \
	     fi.w1.wpa_supplicant1.xml \
	     net.hadess.SensorProxy.xml \
	     gclue-marshal.list

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = geoclue-$(GEOCLUE_API_VERSION).pc

DISTCLEANFILES = $(pkgconfig_DATA)

-include $(top_srcdir)/git.mk
