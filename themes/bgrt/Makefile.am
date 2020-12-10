themedir = $(datadir)/plymouth/themes/bgrt
nodist_theme_DATA = bgrt.plymouth

MAINTAINERCLEANFILES = Makefile.in bgrt.plymouth
CLEANFILES = bgrt.plymouth

bgrt.plymouth: $(srcdir)/bgrt.plymouth.in
	sed -e 's,[@]PLYMOUTH_THEME_PATH[@],$(PLYMOUTH_THEME_PATH),g' \
	       $(srcdir)/bgrt.plymouth.in > bgrt.plymouth

EXTRA_DIST = bgrt.plymouth.in
