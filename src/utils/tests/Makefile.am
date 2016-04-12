noinst_PROGRAMS = test-utils

test_utils_SOURCES = test-utils.c

test_utils_CPPFLAGS = \
	-I $(top_srcdir)/src/utils \
	-DLIBNM_BUILD \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	"-I${top_srcdir}/shared/"

test_utils_LDADD = \
	${top_builddir}/src/utils/libutils-libnm.la \
	$(GTK_LIBS) \
	$(LIBNM_LIBS)

TESTS = $(noinst_PROGRAMS)

