# This is used in PANEL_CFLAGS
cappletname = datetime

uidir = $(pkgdatadir)/ui/datetime
dist_ui_DATA = datetime-dialog.ui

icons16dir = $(datadir)/icons/hicolor/16x16/apps
dist_icons16_DATA = icons/16x16/unity-datetime-panel.png
icons22dir = $(datadir)/icons/hicolor/22x22/apps
dist_icons22_DATA = icons/22x22/unity-datetime-panel.png
icons32dir = $(datadir)/icons/hicolor/32x32/apps
dist_icons32_DATA = icons/32x32/unity-datetime-panel.png
icons48dir = $(datadir)/icons/hicolor/48x48/apps
dist_icons48_DATA = icons/48x48/unity-datetime-panel.png
icons256dir = $(datadir)/icons/hicolor/256x256/apps
dist_icons256_DATA = icons/256x256/unity-datetime-panel.png
iconsscalabledir = $(datadir)/icons/hicolor/scalable/apps
dist_iconsscalable_DATA = icons/scalable/unity-datetime-panel.svg

INCLUDES =						\
	$(PANEL_CFLAGS)					\
	$(DATETIME_PANEL_CFLAGS) \
	-DLOCALEDIR="\"$(datadir)/locale\"" \
	-DPKGDATADIR="\"$(pkgdatadir)\"" \
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libdatetime.la

libdatetime_la_SOURCES = \
	datetime-prefs.c \
	datetime-prefs-locations.c \
	datetime-prefs-locations.h \
	settings-shared.h \
	utils.c \
	utils.h

libdatetime_la_LIBADD = $(PANEL_LIBS) $(DATETIME_PANEL_LIBS)
libdatetime_la_LDFLAGS = $(PANEL_LDFLAGS)

polkitdir = $(datadir)/polkit-1/actions
polkit_in_files = com.canonical.controlcenter.datetime.policy.in

@INTLTOOL_POLICY_RULE@
polkit_DATA = $(polkit_in_files:.policy.in=.policy)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = unity-datetime-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

EXTRA_DIST =				\
	$(polkit_in_files)

-include $(top_srcdir)/git.mk
