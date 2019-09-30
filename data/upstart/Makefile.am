
xdg_autostartdir = $(datadir)/upstart/xdg/autostart
xdg_autostart_DATA = indicator-messages.desktop

%.desktop: %.desktop.in
	$(AM_V_GEN) sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

EXTRA_DIST = \
	indicator-messages.desktop.in

CLEANFILES = \
	$(xdg_autostart_DATA)
