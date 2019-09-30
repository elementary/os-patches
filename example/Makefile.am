VER=3

noinst_PROGRAMS =               \
	messagedialog		\
        menus

messagedialog_SOURCES =		\
	messagedialog.c

menus_SOURCES =                 \
        menus.c

messagedialog_CPPFLAGS =	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/src	\
	$(GCC_FLAGS)		\
	$(GTK_CFLAGS)		\
	$(MAINTAINER_CFLAGS)

menus_CPPFLAGS =                \
        -I$(top_srcdir)         \
        -I$(top_srcdir)/src     \
        -I$(top_builddir)/src   \
        $(GCC_FLAGS)            \
        $(GTK_CFLAGS)           \
        $(MAINTAINER_CFLAGS)

messagedialog_LDADD = $(top_builddir)/src/libido$(VER)-0.1.la $(GTK_LIBS)

menus_LDADD = $(top_builddir)/src/libido$(VER)-0.1.la $(GTK_LIBS)
