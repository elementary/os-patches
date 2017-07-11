SUBDIRS = appdata

INCLUDES =					\
	-I$(top_srcdir)				\
	$(SHELL_CFLAGS)				\
	$(CHEESE_CFLAGS)

bin_PROGRAMS = unity-control-center

MARSHAL_FILES = cc-shell-marshal.c cc-shell-marshal.h
BUILT_SOURCES = $(MARSHAL_FILES)

cc-shell-marshal.h: cc-shell-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=cc_shell_marshal $< --header > $@

cc-shell-marshal.c: cc-shell-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=cc_shell_marshal $< --body --header > $@

unity_control_center_SOURCES =			\
	control-center.c			\
	cc-shell-log.c				\
	cc-shell-log.h				\
	gnome-control-center.c			\
	gnome-control-center.h			\
	cc-shell-category-view.c		\
	cc-shell-category-view.h		\
	cc-shell-item-view.c			\
	cc-shell-item-view.h			\
	cc-shell-model.c			\
	cc-shell-model.h			\
	cc-shell-nav-bar.c			\
	cc-shell-nav-bar.h			\
	$(MARSHAL_FILES)

unity_control_center_LDADD =			\
	libunity-control-center.la		\
	$(SHELL_LIBS)				\
	$(CHEESE_LIBS)

unity_control_center_LDFLAGS = -export-dynamic

lib_LTLIBRARIES = libunity-control-center.la

libunity_control_center_include_HEADERS =      \
	cc-panel.h                              \
	cc-shell.h                              \
	cc-editable-entry.h			\
	$(NULL)

libunity_control_center_la_SOURCES =		\
	cc-panel.c				\
	cc-panel.h				\
	cc-shell.c				\
	cc-shell.h				\
	cc-editable-entry.c			\
	cc-editable-entry.h			\
	$(NULL)

libunity_control_center_la_LDFLAGS =		\
	-no-undefined				\
	-version-info $(LIBUNITYCONTROLCENTER_CURRENT):$(LIBUNITYCONTROLCENTER_REVISION):$(LIBUNITYCONTROLCENTER_AGE) \
	$(NULL)

libunity_control_center_la_LIBADD = 		\
	$(LIBUNITY_CONTROL_CENTER_LIBS)		\
	$(NULL)

libunity_control_center_la_LIBTOOLFLAGS = --tag=disable-static

libunity_control_center_includedir = $(includedir)/unity-control-center-1/libunity-control-center

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libunity-control-center.pc

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""		\
	-DUIDIR="\"$(uidir)\""					\
	-DMENUDIR="\"$(menudir)\""				\
	-DPANELS_DIR="\"$(PANELS_DIR)\""

menudir = $(sysconfdir)/xdg/menus
menu_DATA = unitycc.menu

unitycc.menu: unitycc.menu.in
	$(AM_V_GEN) cat $< | sed 's,@applicationsdir@,$(datadir)/applications/,' > $@

uidir = $(pkgdatadir)/ui
ui_DATA = shell.ui

sysdir = $(datadir)/applications
sys_in_files = unity-control-center.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

directorydir = $(datadir)/desktop-directories
directory_in_files = unitycc.directory.in
directory_DATA = $(directory_in_files:.directory.in=.directory)
@INTLTOOL_DIRECTORY_RULE@

completiondir = $(datadir)/bash-completion/completions
completion_in_files = completions/unity-control-center.in
completion_DATA = $(completion_in_files:.in=)
completions/unity-control-center: completions/unity-control-center.in list-panel.sh
	$(AM_V_GEN) mkdir -p `dirname $@` && cat $< | sed "s,@PANELS@,`$(srcdir)/list-panel.sh $(top_srcdir)`," > $@

EXTRA_DIST =					\
	$(ui_DATA)				\
	unitycc.directory.in			\
	unitycc.menu.in			   	\
	unity-control-center.desktop.in.in	\
	libunity-control-center.pc.in  \
	cc-shell-marshal.list

DISTCLEANFILES = unity-control-center.desktop unity-control-center.desktop.in unitycc.directory unitycc.menu

-include $(top_srcdir)/git.mk
