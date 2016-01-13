SUBDIRS=. tests

noinst_LTLIBRARIES = \
	libgconf-helpers.la \
	libgconf-helpers-test.la

HELPERS_SOURCES = \
	gconf-helpers.h \
	gconf-helpers.c \
	gconf-upgrade.h \
	gconf-upgrade.c

libgconf_helpers_la_SOURCES = $(HELPERS_SOURCES)

libgconf_helpers_la_CPPFLAGS = \
	-I$(top_srcdir)/src/utils \
	$(GTK_CFLAGS) \
	$(NMA_CFLAGS) \
	$(MIGRATION_CFLAGS) \
	$(DISABLE_DEPRECATED)

libgconf_helpers_la_LIBADD = \
	$(GTK_LIBS) \
	$(NMA_LIBS) \
	$(MIGRATION_LIBS)

#########################
# Test library
#########################

libgconf_helpers_test_la_SOURCES = $(HELPERS_SOURCES)

libgconf_helpers_test_la_CPPFLAGS = \
	-I$(top_srcdir)/src/utils \
	$(GTK_CFLAGS) \
	$(NMA_CFLAGS) \
	$(MIGRATION_CFLAGS) \
	$(DISABLE_DEPRECATED)

# no keyring or gconf libs since we'll override them
libgconf_helpers_test_la_LIBADD = \
	$(NMA_LIBS)

