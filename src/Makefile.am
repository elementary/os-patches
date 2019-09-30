CLEANFILES =

VER=3
lib_LTLIBRARIES = libido3-0.1.la

ido_built_public_sources =		\
	idotypebuiltins.h

stamp_files =				\
	idotypebuiltins.h		\
	idotypebuiltins.c

sources_h =				\
	idoalarmmenuitem.h		\
	idocalendarmenuitem.h		\
	idoentrymenuitem.h		\
	idomessagedialog.h		\
	idorange.h			\
	idoscalemenuitem.h		\
	idoswitchmenuitem.h		\
	idousermenuitem.h		\
	idoappointmentmenuitem.h	\
	idobasicmenuitem.h		\
	idoprogressmenuitem.h		\
	idotimestampmenuitem.h		\
	idolocationmenuitem.h		\
	idotimeline.h			\
	libido.h			\
	idoactionhelper.h		\
	idomediaplayermenuitem.h	\
	idoplaybackmenuitem.h		\
	idoapplicationmenuitem.h 	\
	idodetaillabel.h		\
	idosourcemenuitem.h

EXTRA_DIST =				\
	ido.list			\
	idotypebuiltins.h.template	\
	idotypebuiltins.c.template


idotypebuiltins.h: stamp-idotypebuiltins.h

stamp-idotypebuiltins.h: $(sources_h)
	( cd $(srcdir) && $(GLIB_MKENUMS) --template idotypebuiltins.h.template \
		$(sources_h) ) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh idotypebuiltins.h || cp xgen-gtbh idotypebuiltins.h ) \
	&& rm -f xgen-gtbh && echo timestamp > $(@F)

idotypebuiltins.c: stamp-idotypebuiltins.h
	( cd $(srcdir) && $(GLIB_MKENUMS) --template idotypebuiltins.c.template \
		$(sources_h) ) > xgen-gtbc \
	&& cp xgen-gtbc idotypebuiltins.c && rm -f xgen-gtbc

INCLUDES =				\
	-I$(srcdir)			\
	-I$(top_srcdir)			\
	-DG_LOG_DOMAIN=\"IDO\"		\
	-DPREFIX=\"$(prefix)"\"		\
	-DLIBDIR=\"$(libdir)"\"		\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGDK_DISABLE_DEPRECATED

AM_CPPFLAGS =				\
	$(GCC_FLAGS)			\
	$(GTK_CFLAGS)			\
	$(MAINTAINER_CFLAGS)    \
	-Wall -Werror -Wextra -Wno-unused-parameter -Wno-error=deprecated-declarations

AM_CFLAGS =                             \
	$(COVERAGE_CFLAGS)

libido_0_1_la_SOURCES =			\
	libido.c			\
	idotypebuiltins.c		\
	idocalendarmenuitem.c		\
	idoalarmmenuitem.c		\
	idoentrymenuitem.c		\
	idomessagedialog.c		\
	idorange.c			\
	idoscalemenuitem.c		\
	idoswitchmenuitem.c		\
	idotimeline.c			\
	idomenuitemfactory.c		\
	idoactionhelper.c		\
	idousermenuitem.c		\
	idomediaplayermenuitem.c	\
	idoplaybackmenuitem.c		\
	idoappointmentmenuitem.c	\
	idobasicmenuitem.c		\
	idoprogressmenuitem.c		\
	idotimestampmenuitem.c		\
	idolocationmenuitem.c		\
	idoapplicationmenuitem.c	\
	idodetaillabel.c		\
	idosourcemenuitem.c

libido3_0_1_la_SOURCES = $(libido_0_1_la_SOURCES)

libidoincludedir=$(includedir)/libido$(VER)-0.1/libido

libidoinclude_HEADERS =			\
	idocalendarmenuitem.h		\
	idoentrymenuitem.h		\
	idomessagedialog.h		\
	idorange.h			\
	idoscalemenuitem.h		\
	idoswitchmenuitem.h		\
	idotimeline.h			\
	libido.h

libido_0_1_la_LIBADD = $(GTK_LIBS) $(LIBM)
libido_0_1_la_LDFLAGS =                 \
	$(GTK_LT_LDFLAGS)               \
	$(COVERAGE_LDFLAGS)
libido3_0_1_la_LIBADD = $(libido_0_1_la_LIBADD)
libido3_0_1_la_LDFLAGS =                \
	$(libido_0_1_la_LDFLAGS)        \
	$(COVERAGE_LDFLAGS)

DISTCLEANFILES =			\
	stamp-idotypebuiltins.h		\
	idotypebuiltins.h		\
	idotypebuiltins.c

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
        --symbol-prefix=ido \
        --warn-all \
        --identifier-prefix=Ido

if HAVE_INTROSPECTION

Ido3-0.1.gir: libido3-0.1.la
Ido3_0_1_gir_INCLUDES = Gtk-3.0
Ido3_0_1_gir_CFLAGS =
Ido3_0_1_gir_LIBS = libido3-0.1.la
Ido3_0_1_gir_FILES = \
	idocalendarmenuitem.h		\
	idoentrymenuitem.h		\
	idomessagedialog.h		\
	idorange.h			\
	idoscalemenuitem.h		\
	idoswitchmenuitem.h		\
	idotimeline.h			\
	$(libido_0_1_la_SOURCES)
Ido3_0_1_gir_NAMESPACE = Ido3
Ido3_0_1_gir_VERSION = 0.1
Ido3_0_1_gir_SCANNER_FLAGS = $(INTROSPECTION_SCANNER_ARGS)

INTROSPECTION_GIRS += Ido3-0.1.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

endif

if HAVE_INTROSPECTION

vapidir = $(datadir)/vala/vapi
vapi_DATA = Ido3-0.1.vapi

Ido3-0.1.vapi: Ido3-0.1.gir
	$(VALA_API_GEN) --library=Ido3-0.1 \
			--pkg gtk+-3.0 \
			$<

CLEANFILES += $(vapi_DATA)

endif
