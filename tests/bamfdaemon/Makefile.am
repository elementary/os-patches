noinst_PROGRAMS = \
	test-bamf

bamf_test_extra_sources = \
	$(top_srcdir)/src/bamf-daemon.c \
	$(top_srcdir)/src/bamf-legacy-window.c \
	$(top_srcdir)/src/bamf-legacy-window-test.c \
	$(top_srcdir)/src/bamf-legacy-screen.c \
	$(top_srcdir)/src/bamf-view.c \
	$(top_srcdir)/src/bamf-control.c \
	$(top_srcdir)/src/bamf-matcher.c \
	$(top_srcdir)/src/bamf-application.c \
	$(top_srcdir)/src/bamf-window.c \
	$(top_srcdir)/src/bamf-tab.c \
	$(top_srcdir)/src/bamf-xutils.c \
	$(NULL)

bamf_test_extra_headers = \
	$(top_srcdir)/src/main.h \
	$(top_srcdir)/src/bamf-daemon.h \
	$(top_srcdir)/src/bamf-legacy-window.h \
	$(top_srcdir)/src/bamf-legacy-window-test.h \
	$(top_srcdir)/src/bamf-legacy-screen.h \
	$(top_srcdir)/src/bamf-view.h \
	$(top_srcdir)/src/bamf-control.h \
	$(top_srcdir)/src/bamf-matcher.h \
	$(top_srcdir)/src/bamf-window.h \
	$(top_srcdir)/src/bamf-tab.h \
	$(top_srcdir)/src/bamf-application.h \
	$(top_srcdir)/src/bamf-xutils.h \
	$(NULL)

test_bamf_SOURCES = \
	$(bamf_test_extra_sources) \
	$(bamf_test_extra_headers) \
	test-bamf.c \
	test-view.c \
	test-application.c \
	test-window.c \
	test-matcher.c

test_bamf_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-DTESTDIR=\""$(abs_top_srcdir)/tests"\" \
	$(COVERAGE_CFLAGS) \
	$(GCC_FLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTOP_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	$(WNCK_CFLAGS) \
	$(X_CFLAGS) \
	$(NULL)

test_bamf_LDADD = \
	$(top_builddir)/lib/libbamf-private/libbamf-private.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(GTOP_LIBS) \
	$(WNCK_LIBS) \
	$(X_LIBS) \
	$(NULL)

test_bamf_LDFLAGS = \
	-Xlinker -export-dynamic -Wl,-O1 -Wl,-Bsymbolic-functions \
	$(COVERAGE_LDFLAGS) \
	$(NULL)

TEST_BINARY = test-bamf
include $(top_srcdir)/tests/Makefile.am.gtests
