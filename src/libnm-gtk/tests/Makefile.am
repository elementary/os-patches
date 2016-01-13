
noinst_PROGRAMS = \
	test-mobile-providers

test_mobile_providers_SOURCES = \
	test-mobile-providers.c

test_mobile_providers_CPPFLAGS = \
	$(NMA_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/libnm-gtk \
	-DTEST_DATA_DIR=\""$(abs_top_srcdir)/src/libnm-gtk/tests"\"

test_mobile_providers_LDADD = \
	$(top_builddir)/src/libnm-gtk/libnm-gtk.la \
	$(NMA_LIBS)

check-local: test-mobile-providers
	$(abs_builddir)/test-mobile-providers


EXTRA_DIST = \
	iso3166-test.xml \
	serviceproviders-test.xml
