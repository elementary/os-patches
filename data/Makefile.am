service_in_files = org.freedesktop.GeoClue2.service.in
systemconf_in_files = geoclue.conf.in
systemdservice_in_files = geoclue.service.in

if BUILD_BACKEND
systemconfdir    = $(sysconfdir)/geoclue
systemconf_DATA = $(systemconf_in_files:.conf.in=.conf)

if BUILD_DEMO_AGENT
$(systemconf_DATA): $(systemconf_in_files) Makefile
	@sed -e \
	"s|\@demo_agent\@|geoclue-demo-agent;|" \
	$< > $@
else
$(systemconf_DATA): $(systemconf_in_files) Makefile
	@sed -e "s|\@demo_agent\@||" $< > $@
endif

servicedir       = $(datadir)/dbus-1/system-services
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< | \
	sed -e "s|\@dbus_srv_user\@|$(dbus_srv_user)|" > $@

# DBus Service policy file
dbusservicedir = $(DBUS_SYS_DIR)
dbusservice_DATA = org.freedesktop.GeoClue2.conf \
		   org.freedesktop.GeoClue2.Agent.conf

if HAVE_SYSTEMD
systemdservicedir       = $(systemdsystemunitdir)
systemdservice_DATA     = $(systemdservice_in_files:.service.in=.service)

$(systemdservice_DATA): $(systemdservice_in_files) Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< | \
	sed -e "s|\@dbus_srv_user\@|$(dbus_srv_user)|" > $@
endif
endif

DISTCLEANFILES = $(service_DATA) \
                 $(systemconf_DATA) \
                 $(dbusservice_DATA) \
                 $(systemdservice_DATA)
EXTRA_DIST = $(service_in_files) \
             $(systemdservice_in_files) \
             $(systemconf_in_files)

-include $(top_srcdir)/git.mk
