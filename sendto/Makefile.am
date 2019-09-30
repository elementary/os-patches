AM_CFLAGS =                                     \
        -DDATADIR=\"$(datadir)\"                \
        -DICONDIR=\"$(icondir)\"                \
	-DLOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/lib                     \
	-I$(top_builddir)/lib                   \
	$(SENDTO_CFLAGS)                        \
        $(NST_CFLAGS)                           \
	$(BLUETOOTH_CFLAGS)			\
	$(DISABLE_DEPRECATED)			\
	$(WARN_CFLAGS)

bin_PROGRAMS = bluetooth-sendto

bluetooth_sendto_SOURCES = main.c obex-agent.c obex-agent.h

bluetooth_sendto_LDADD = $(top_builddir)/lib/libgnome-bluetooth.la $(SENDTO_LIBS)

man_MANS = bluetooth-sendto.1

desktopdir = $(datadir)/applications
desktop_in_in_files = bluetooth-sendto.desktop.in.in
desktop_in_files = bluetooth-sendto.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(man_MANS) $(desktop_in_in_files)
DISTCLEANFILES = bluetooth-sendto.desktop

MAINTAINERCLEANFILES = Makefile.in
