include $(top_srcdir)/Makefile.decl

interfacedir = $(datadir)/dbus-1/interfaces
interface_DATA = org.freedesktop.GeoClue2.Agent.xml

docs-org.freedesktop.GeoClue2.Agent.xml: Makefile.am $(interface_DATA)
	$(AM_V_GEN)$(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.GeoClue2.	\
		--c-namespace GClue				\
    		--generate-docbook=docs				\
		--generate-c-code geoclue-agent-interface	\
		$(srcdir)/$(interface_DATA)
noinst_DATA = docs-org.freedesktop.GeoClue2.Agent.xml

dbus_built_sources = geoclue-agent-interface.c geoclue-agent-interface.h
geoclue-agent-interface.c: geoclue-agent-interface.h
geoclue-agent-interface.h: Makefile.am $(noinst_DATA) $(interface_data)

BUILT_SOURCES = $(dbus_built_sources) \
		$(NULL)

AM_CPPFLAGS = $(GEOCLUE_CFLAGS) 		       \
	      $(WARN_CFLAGS)			       \
	      -DLOCALEDIR="\"$(datadir)/locale\""      \
	      -DG_LOG_DOMAIN=\""Geoclue"\"	       \
	      -DABS_TOP_SRCDIR=\""$(abs_top_srcdir)"\" \
	      -DSYSCONFDIR=\""$(sysconfdir)"\"

noinst_LTLIBRARIES = libgeoclue-agent.la
libgeoclue_agent_la_SOURCES = $(BUILT_SOURCES)

CLEANFILES = $(noinst_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(interface_DATA)

-include $(top_srcdir)/git.mk
