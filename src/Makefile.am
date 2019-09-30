LIBBAMF_PRIVATE_LIB = $(top_builddir)/lib/libbamf-private/libbamf-private.la
bamfdaemondir = $(pkglibexecdir)

bamfdaemon_PROGRAMS = \
	bamfdaemon \
	$(NULL)

bamfdaemon_sources = \
	main.c \
	bamf-daemon.c \
	bamf-legacy-window.c \
	bamf-legacy-window-test.c \
	bamf-legacy-screen.c \
	bamf-view.c \
	bamf-control.c \
	bamf-matcher.c \
	bamf-application.c \
	bamf-window.c \
	bamf-tab.c \
	bamf-xutils.c \
	$(NULL)

bamfdaemon_headers = \
	main.h \
	bamf-daemon.h \
	bamf-legacy-window.h \
	bamf-legacy-window-test.h \
	bamf-legacy-screen.h \
	bamf-legacy-screen-private.h \
	bamf-view.h \
	bamf-control.h \
	bamf-matcher.h \
	bamf-matcher-private.h \
	bamf-window.h \
	bamf-application.h \
	bamf-tab.h \
	bamf-xutils.h \
	$(NULL)

bamfdaemon_SOURCES = \
	$(bamfdaemon_sources) \
	$(bamfdaemon_headers) \
	$(NULL)

bamfdaemon_LDADD = \
	$(LIBBAMF_PRIVATE_LIB) \
	$(X_LIBS) \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(WNCK_LIBS) \
	$(GTOP_LIBS) \
	$(NULL)

bamfdaemon_CFLAGS = \
	-Wall -std=c99 \
	-DDATADIR=\""$(datadir)"\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-I$(srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(WNCK_CFLAGS) \
	$(GTOP_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	$(NULL)

bamfdaemon_LDFLAGS = \
	-Xlinker -export-dynamic -Wl,-O1 -Wl,-Bsymbolic-functions \
	$(COVERAGE_LDFLAGS) \
	$(NULL)
