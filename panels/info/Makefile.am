cappletname = info

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(INFO_PANEL_CFLAGS)				\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(datadir)\""			\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	$(NULL)

noinst_PROGRAMS = test-hostname logo-generator
test_hostname_SOURCES = hostname-helper.c hostname-helper.h test-hostname.c
test_hostname_LDADD = $(PANEL_LIBS) $(INFO_PANEL_LIBS)
test_hostname_CFLAGS = $(INCLUDES)

logo_generator_SOURCES = logo-generator.vala

logo_generator_VALAFLAGS = \
	--pkg cairo        \
	--pkg pango        \
	--pkg pangocairo   \
	--target-glib 2.32

logo_generator_CFLAGS = $(PANEL_CFLAGS) $(INFO_PANEL_CFLAGS)
logo_generator_LDADD = $(PANEL_LIBS) $(INFO_PANEL_LIBS)

all-local: check-local

check-local: test-hostname
	$(builddir)/test-hostname $(srcdir)/hostnames-test.txt > /dev/null

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libinfo.la

libinfo_la_SOURCES =		\
	info-module.c		\
	cc-info-panel.c		\
	cc-info-panel.h		\
	hostname-helper.c	\
	hostname-helper.h	\
	gsd-disk-space-helper.h	\
	gsd-disk-space-helper.c

libinfo_la_LIBADD = $(PANEL_LIBS) $(INFO_PANEL_LIBS)
libinfo_la_LDFLAGS = $(PANEL_LDFLAGS)

legaldir = $(pkgdatadir)
dist_legal_DATA = searchingthedashlegalnotice.html

uidir = $(pkgdatadir)/ui
dist_ui_DATA = info.ui GnomeLogoVerticalMedium.svg UbuntuLogo.png

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = unity-info-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

SPACEDIR=$(top_srcdir)/../gnome-settings-daemon/plugins/housekeeping/
SPACEFILES=gsd-disk-space-helper.c gsd-disk-space-helper.h
update-from-gsd:
	FILES="$(SPACEFILES)" DIR="$(SPACEDIR)" $(top_srcdir)/update-from-gsd.sh && changed=true ; \
	git commit -m "info: Update from gnome-settings-daemon" $(SPACEFILES)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)
EXTRA_DIST = hostnames-test.txt

-include $(top_srcdir)/git.mk
