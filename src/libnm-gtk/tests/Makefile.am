
noinst_PROGRAMS = \
	test-mobile-providers

test_mobile_providers_SOURCES = \
	test-mobile-providers.c

test_mobile_providers_CPPFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBNM_GLIB_CFLAGS) \
	"-I${top_srcdir}/shared/" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/libnm-gtk \
	-DTEST_DATA_DIR=\""$(abs_top_srcdir)/src/libnm-gtk/tests"\"

test_mobile_providers_LDADD = \
	$(top_builddir)/src/libnm-gtk/libnm-gtk.la \
	$(LIBNM_GLIB_LIBS)

TESTS = $(noinst_PROGRAMS)

EXTRA_DIST = \
	iso3166-test.xml \
	serviceproviders-test.xml
