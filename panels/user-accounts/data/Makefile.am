SUBDIRS = faces

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
Desktop_in_files = unity-user-accounts-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = 					\
	unity-user-accounts-panel.desktop.in.in	\
	org.freedesktop.realmd.xml

CLEANFILES =					\
	unity-user-accounts-panel.desktop	\
	$(NULL)

DISTCLEANFILES =				\
	$(NULL)

-include $(top_srcdir)/git.mk
