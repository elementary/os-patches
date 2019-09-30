## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)

test_PROGRAMS = \
	accounts-glib-testsuite \
	test-process

check_PROGRAMS = \
	accounts-glib-testsuite \
	test-process

if HAVE_XMLLINT
xml_test = xml-dtd-validate.sh
else
xml_test =
endif

test_SCRIPTS = \
	accounts-glib-test.sh

dist_check_SCRIPTS = \
	accounts-glib-test.sh \
	$(xml_test)

accounts_glib_testsuite_SOURCES = check_ag.c
accounts_glib_testsuite_CPPFLAGS = \
	$(CHECK_CFLAGS) \
	-I$(top_builddir) \
	$(AM_CPPFLAGS)
accounts_glib_testsuite_LDADD = \
	$(CHECK_LIBS) \
	$(top_builddir)/libaccounts-glib/libaccounts-glib.la

test_process_SOURCES = test-process.c
test_process_CPPFLAGS = \
	$(LIBACCOUNTS_CFLAGS) \
	$(AM_CPPFLAGS)
test_process_LDADD = \
	$(LIBACCOUNTS_LIBS) \
	$(top_builddir)/libaccounts-glib/libaccounts-glib.la

TESTS_ENVIRONMENT = \
	DTDDIR=$(top_srcdir)/libaccounts-glib/ \
	TESTDATADIR=$(top_srcdir)/tests/ \
	XMLLINT=$(XMLLINT)

TESTS = $(dist_check_SCRIPTS)

tests.xml: tests.xml.in Makefile
	$(SED) -e 's|@''TESTDIR''@|$(testdir)|' \
	       -e 's|@''TESTDATADIR''@|$(testdatadir)|' $< > $@
CLEANFILES = tests.xml

dist_noinst_DATA = \
	tests.xml.in

test_DATA = \
	tests.xml

dist_testdata_DATA = \
	e-mail.service-type \
	Gallery.application \
	Mailer.application \
	MyProvider.provider \
	MyService.service \
	MyService2.service \
	OtherService.service \
	maemo.provider

nobase_dist_testdata_DATA = \
	applications/Mailer.desktop

installcheck-local: $(TESTS)
	cd "$(testdir)" && TESTDATADIR="$(testdatadir)" ./accounts-glib-test.sh
