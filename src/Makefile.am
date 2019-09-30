CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST = 

include $(top_srcdir)/Makefile.am.marshal

##################################
# Indicator
##################################

applicationlibdir = $(INDICATORDIR)
applicationlib_LTLIBRARIES = libapplication.la
libapplication_la_SOURCES = \
	application-service-marshal.c \
	dbus-shared.h \
	indicator-application.c
libapplication_la_CFLAGS = $(INDICATOR_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-Wall \
	-Wl,-Bsymbolic-functions \
	-Wl,-z,defs \
	-Wl,--as-needed \
	-Werror -Wno-error=deprecated-declarations \
	-DG_LOG_DOMAIN=\"Indicator-Application\"
libapplication_la_LIBADD = $(INDICATOR_LIBS) 
libapplication_la_LDFLAGS = $(COVERAGE_LDFLAGS) \
	-module -avoid-version

##################################
# Service
##################################

pkglibexec_PROGRAMS = indicator-application-service

BUILT_SOURCES += \
	application-service-marshal.h \
	application-service-marshal.c \
	notification-watcher-server.h

indicator_application_service_SOURCES = \
	application-service.c \
	application-service-appstore.h \
	application-service-appstore.c \
	application-service-marshal.c \
	application-service-watcher.h \
	application-service-watcher.c \
	gen-application-service.xml.c \
	dbus-shared.h \
	generate-id.h \
	generate-id.c

indicator_application_service_CFLAGS = \
	$(INDICATOR_CFLAGS) \
	$(APPINDICATOR_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-DDATADIR="\"$(pkgdatadir)\"" \
	-Wall -Werror -Wno-error=deprecated-declarations \
	-DG_LOG_DOMAIN=\"indicator-application-service\"

indicator_application_service_LDADD = \
	$(INDICATOR_LIBS) \
	$(APPINDICATOR_LIBS)

indicator_application_service_LDFLAGS = \
	$(COVERAGE_LDFLAGS)

glib_marshal_list = application-service-marshal.list
glib_marshal_prefix = _application_service_marshal

##################################
# DBus Specs
##################################

GDBUS_SPECS = \
	application-service.xml

DBUS_SPECS = \
	notification-watcher.xml

%-client.h: %.xml
	dbus-binding-tool \
		--prefix=_$(notdir $(subst -,_,$(<:.xml=)))_client \
		--mode=glib-client \
		--output=$@ \
		$<

%-server.h: %.xml
	dbus-binding-tool \
		--prefix=_$(notdir $(subst -,_,$(<:.xml=)))_server \
		--mode=glib-server \
		--output=$@ \
		$<

gen-%.xml.c: %.xml
	@echo "Building $@ from $<"
	@echo "const char * _$(notdir $(subst -,_,$(subst .,_,$(basename $<)))) = " > $@
	@sed -e "s:\":\\\\\":g" -e s:^:\": -e s:\$$:\\\\n\": $< >> $@
	@echo ";" >> $@

gen-%.xml.h: %.xml
	@echo "Building $@ from $<"
	@echo "extern const char * _$(notdir $(subst -,_,$(subst .,_,$(basename $<))));" > $@

BUILT_SOURCES += \
	$(DBUS_SPECS:.xml=-client.h) \
	$(DBUS_SPECS:.xml=-server.h) \
	gen-application-service.xml.c \
	gen-application-service.xml.h

CLEANFILES += $(BUILT_SOURCES)

EXTRA_DIST += $(DBUS_SPECS) $(GDBUS_SPECS)
