noinst_LTLIBRARIES = libwizard.la

libwizard_la_SOURCES = pin.c pin.h

libwizard_la_LIBADD = $(BLUETOOTH_PANEL_LIBS)
libwizard_la_LDFLAGS = -rdynamic

bin_PROGRAMS = bluetooth-wizard

bluetooth_wizard_SOURCES = main.c

bluetooth_wizard_LDADD = $(builddir)/../lib/libgnome-bluetooth.la libwizard.la $(BLUETOOTH_PANEL_LIBS) $(LIBM)
bluetooth_wizard_LDFLAGS = -rdynamic

AM_CFLAGS = $(BLUETOOTH_PANEL_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED) -DPKGDATADIR="\"$(pkgdatadir)\""

pin_DATA = pin-code-database.xml
pindir = $(pkgdatadir)

ui_DATA = wizard.ui
uidir = $(pkgdatadir)

INCLUDES = \
	 -I$(srcdir)/../lib \
	-DLOCALEDIR="\"$(datadir)/locale\""

man_MANS = bluetooth-wizard.1

all: check

check:
	@if test -n $(XMLLINT) ; then \
		xmllint --noout --valid $(srcdir)/pin-code-database.xml ;	\
	fi

EXTRA_DIST = $(man_MANS) $(pin_DATA) $(ui_DATA)

MAINTAINERCLEANFILES = Makefile.in
