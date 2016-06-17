
xdg_autostartdir = $(pkgdatadir)/upstart/xdg/autostart
xdg_autostart_DATA = indicator-application.desktop

%.desktop: %.desktop.in
	sed -e "s|\@libexecdir\@|$(pkglibexecdir)|" $< > $@

EXTRA_DIST = \
	indicator-application.desktop.in
