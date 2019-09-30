noinst_LTLIBRARIES = libwizard.la

libwizard_la_SOURCES = pin.c pin.h

libwizard_la_LIBADD = $(WIZARD_LIBS)
libwizard_la_LDFLAGS = -no-undefined $(AM_LDFLAGS)

# noinst_PROGRAMS = test-input

bin_PROGRAMS = bluetooth-wizard

bluetooth_wizard_SOURCES = main.c

bluetooth_wizard_LDADD = $(top_builddir)/lib/libgnome-bluetooth.la libwizard.la $(WIZARD_LIBS) $(LIBM)

# test_input_SOURCES = test-input.c bluetooth-input.c bluetooth-input.h
# test_input_LDADD = $(INPUT_LIBS)

AM_CFLAGS = $(WIZARD_CFLAGS) $(INPUT_CFLAGS) $(WARN_CFLAGS) $(DISABLE_DEPRECATED) -DPKGDATADIR="\"$(pkgdatadir)\""

pin_DATA = pin-code-database.xml
pindir = $(pkgdatadir)

ui_DATA = wizard.ui
uidir = $(pkgdatadir)

desktopdir = $(datadir)/applications
desktop_in_in_files = bluetooth-wizard.desktop.in.in
desktop_in_files = bluetooth-wizard.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

INCLUDES = -I$(top_srcdir)/lib

man_MANS = bluetooth-wizard.1

all: check

check:
	@if test -n $(XMLLINT) ; then \
		xmllint --noout --valid $(srcdir)/pin-code-database.xml ;	\
	fi

EXTRA_DIST = $(man_MANS) $(pin_DATA) $(ui_DATA) $(desktop_in_in_files)
CLEANFILES = $(desktop_DATA)

MAINTAINERCLEANFILES = Makefile.in
