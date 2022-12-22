AM_CPPFLAGS = -I$(top_srcdir)                                                 \
           -I$(srcdir)/../../../libply                                        \
           -I$(srcdir)/../../../libply-splash-core                            \
           -I$(srcdir)/../../..                                               \
           -I$(srcdir)/../..                                                  \
           -I$(srcdir)/..                                                     \
           -I$(srcdir)

plugindir = $(libdir)/plymouth
plugin_LTLIBRARIES = tribar.la

tribar_la_CFLAGS = $(PLYMOUTH_CFLAGS)                                           \
                 -DPLYMOUTH_BACKGROUND_COLOR=$(background_color)              \
                 -DPLYMOUTH_BACKGROUND_END_COLOR=$(background_end_color)      \
                 -DPLYMOUTH_BACKGROUND_START_COLOR=$(background_start_color)

tribar_la_LDFLAGS = -module -avoid-version -export-dynamic
tribar_la_LIBADD = $(PLYMOUTH_LIBS)                                            \
                 ../../../libply/libply.la                                   \
                 ../../../libply-splash-core/libply-splash-core.la
tribar_la_SOURCES = $(srcdir)/plugin.c

MAINTAINERCLEANFILES = Makefile.in
