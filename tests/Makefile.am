SUBDIRS = autopilot
TESTS = indicator-keyboard-test

check_PROGRAMS = indicator-keyboard-tests

AM_CFLAGS = -w
AM_LDFLAGS = -lm
AM_VALAFLAGS = --metadatadir $(top_srcdir)/deps \
               --vapidir $(top_srcdir)/deps

indicator_keyboard_tests_SOURCES = main.vala    \
                                   fixture.vala \
                                   config.vala
indicator_keyboard_tests_VALAFLAGS = $(AM_VALAFLAGS) \
                                     --pkg gio-2.0
indicator_keyboard_tests_CFLAGS = $(AM_CFLAGS)  \
                                  $(GIO_CFLAGS)
indicator_keyboard_tests_LDFLAGS = $(AM_LDFLAGS) \
                                   $(GIO_LIBS)
