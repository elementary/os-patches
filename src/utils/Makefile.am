SUBDIRS= . tests

noinst_LTLIBRARIES = \
	libutils-libnm-glib.la \
	libutils-libnm.la

libutils_libnm_glib_la_SOURCES = \
	utils.c \
	utils.h

libutils_libnm_glib_la_CPPFLAGS = \
	-DLIBNM_GLIB_BUILD \
	$(GTK_CFLAGS) \
	$(LIBNM_GLIB_CFLAGS) \
	"-I${top_srcdir}/shared/"

libutils_libnm_glib_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBNM_GLIB_LIBS)

libutils_libnm_la_SOURCES = \
	utils.c \
	utils.h

libutils_libnm_la_CPPFLAGS = \
	-DLIBNM_BUILD \
	$(GTK_CFLAGS) \
	$(LIBNM_CFLAGS) \
	"-I${top_srcdir}/shared/"

libutils_libnm_la_LIBADD = \
	$(GTK_LIBS) \
	$(LIBNM_LIBS)
