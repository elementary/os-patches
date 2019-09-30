BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =

# the indicator bus file
indicatorsdir = $(datadir)/unity/indicators
dist_indicators_DATA = com.canonical.indicator.bluetooth

# the upstart job file
upstart_jobsdir = $(datadir)/upstart/sessions
upstart_jobs_DATA = indicator-bluetooth.conf
upstart_jobs_in = $(upstart_jobs_DATA:.conf=.conf.in)
$(upstart_jobs_DATA): $(upstart_jobs_in)
	$(AM_V_GEN) $(SED) -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@
BUILT_SOURCES += $(upstart_jobs_DATA)
EXTRA_DIST += $(upstart_jobs_in)
CLEANFILES += $(upstart_jobs_DATA)

# the upstart version of the xdg autostart job file
# see https://code.launchpad.net/~charlesk/indicator-bluetooth/upstart-job/+merge/198100
upstart_xdg_autostartdir = $(datadir)/upstart/xdg/autostart
upstart_xdg_autostart_DATA = indicator-bluetooth.desktop
upstart_xdg_autostart_in = indicator-bluetooth.upstart.desktop.in
$(upstart_xdg_autostart_DATA): $(upstart_xdg_autostart_in)
	$(AM_V_GEN) $(SED) -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@
BUILT_SOURCES += $(upstart_xdg_autostart_DATA)
EXTRA_DIST += $(upstart_xdg_autostart_in)
CLEANFILES += $(upstart_xdg_autostart_DATA)

# the xdg autostart job file
xdg_autostartdir = /etc/xdg/autostart
xdg_autostart_DATA = indicator-bluetooth.desktop
xdg_autostart_in = $(xdg_autostart_DATA:.desktop=.desktop.in)
$(xdg_autostart_DATA): $(xdg_autostart_in)
	$(AM_V_GEN) $(SED) -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@
BUILT_SOURCES += $(xdg_autostart_DATA)
EXTRA_DIST += $(xdg_autostart_in)
CLEANFILES += $(xdg_autostart_DATA)

# the gsettings schema
gsettings_SCHEMAS = com.canonical.indicator.bluetooth.gschema.xml
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST += \
  com.canonical.indicator.bluetooth.gschema.xml.in \
  indicator-bluetooth.conf.in \
  indicator-bluetooth.desktop.in \
  indicator-bluetooth.upstart.desktop.in

CLEANFILES += \
  $(dbus_services_DATA) \
  $(gsettings_SCHEMAS)
