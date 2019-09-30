SUBDIRS = icons upstart

xdg_autostartdir = /etc/xdg/autostart
xdg_autostart_DATA = indicator-messages.desktop

%.desktop: %.desktop.in
	$(AM_V_GEN) sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

upstart_jobsdir = $(datadir)/upstart/sessions/
upstart_jobs_DATA = indicator-messages.conf

%.conf: %.conf.in
	$(AM_V_GEN) sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

gsettings_SCHEMAS = com.canonical.indicator.messages.gschema.xml
@GSETTINGS_RULES@

indicatordir = $(INDICATOR_DIR)
dist_indicator_DATA = com.canonical.indicator.messages

EXTRA_DIST = \
	indicator-messages.desktop.in \
	indicator-messages.conf.in \
	$(gsettings_SCHEMAS)

CLEANFILES = \
	$(xdg_autostart_DATA) \
	$(upstart_jobs_DATA)
