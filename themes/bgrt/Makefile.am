themedir = $(datadir)/plymouth/themes/bgrt
nodist_theme_DATA = bgrt.plymouth

MAINTAINERCLEANFILES = Makefile.in bgrt.plymouth
CLEANFILES = bgrt.plymouth

bgrt.plymouth: $(srcdir)/bgrt.plymouth.desktop
	$(AM_V_GEN) \
	sed -e 's,[@]PLYMOUTH_THEME_PATH[@],$(PLYMOUTH_THEME_PATH),g' \
	$(srcdir)/bgrt.plymouth.desktop | \
	    $(MSGFMT) --desktop --template /proc/self/fd/0 -d $(top_srcdir)/po -o $@

EXTRA_DIST = bgrt.plymouth.desktop
