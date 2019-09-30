## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir) $(LIBACCOUNTS_CFLAGS)

bin_PROGRAMS = \
    ag-backup \
    ag-tool

ag_tool_SOURCES = main.c
ag_tool_CPPFLAGS = \
	$(AM_CPPFLAGS)
ag_tool_LDADD = \
        $(LIBACCOUNTS_LIBS) \
        $(top_builddir)/libaccounts-glib/libaccounts-glib.la

ag_backup_SOURCES = backup.c
ag_backup_CPPFLAGS = \
	-I$(top_builddir) \
	$(AM_CPPFLAGS)
ag_backup_LDADD = \
	$(LIBACCOUNTS_LIBS)
