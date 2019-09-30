noinst_PROGRAMS = \
	test-libbamf

test_libbamf_SOURCES = \
	test-libbamf.c \
	test-application.c \
	test-matcher.c \
	$(NULL)

test_libbamf_CFLAGS = \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib/libbamf \
	-I$(top_builddir)/lib/libbamf \
	-DTESTDIR=\""$(abs_top_srcdir)/tests"\" \
	$(COVERAGE_CFLAGS) \
	$(GCC_FLAGS) \
	$(GLIB_CFLAGS) \
	$(NULL)

test_libbamf_LDADD = \
	$(top_builddir)/lib/libbamf/libbamf$(LIBBAMF_VER).la \
	$(COVERAGE_LDFLAGS) \
	$(GLIB_LIBS) \
	$(NULL)

TEST_BINARY = test-libbamf
include $(top_srcdir)/tests/Makefile.am.gtests
