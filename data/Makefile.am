
SUBDIRS = upstart

upstart_jobsdir = $(datadir)/upstart/sessions/
upstart_jobs_DATA = indicator-application.conf

%.conf: %.conf.in
	$(AM_V_GEN) sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

xdg_autostartdir = /etc/xdg/autostart
xdg_autostart_DATA = indicator-application.desktop

%.desktop: %.desktop.in
	$(AM_V_GEN) sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

overridedir = $(pkgdatadir)
override_DATA = ordering-override.keyfile

EXTRA_DIST = \
	indicator-application.desktop.in \
	indicator-application.conf.in \
	ordering-override.keyfile

CLEANFILES = \
	indicator-application.conf \
	indicator-application.desktop

