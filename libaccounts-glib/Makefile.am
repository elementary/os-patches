lib_LTLIBRARIES = \
	libaccounts-glib.la

libaccounts_glib_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(builddir) \
	-DG_LOG_DOMAIN=\"accounts-glib\"
libaccounts_glib_la_CFLAGS = \
	$(LIBACCOUNTS_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-Wall -Werror
libaccounts_glib_la_LIBADD = \
	$(LIBACCOUNTS_LIBS)
libaccounts_glib_la_LDFLAGS = \
	$(COVERAGE_LDFLAGS) \
	-version-info 1:3:1 \
	--no-allow-shlib-undefined \
	-export-symbols-regex '^ag_'

BUILT_SOURCES = \
	ag-marshal.c \
	ag-marshal.h

CLEANFILES = $(BUILT_SOURCES)

libaccounts_glib_la_SOURCES = \
	ag-account.h \
	ag-account.c \
	ag-account-service.c \
	ag-application.h \
	ag-application.c \
	ag-auth-data.c \
	ag-debug.h \
	ag-debug.c \
	ag-errors.h \
	ag-internals.h \
	ag-manager.h \
	ag-manager.c \
	ag-provider.h \
	ag-provider.c \
	ag-service.h \
	ag-service.c \
	ag-service-type.h \
	ag-service-type.c \
	ag-types.h \
	ag-util.h \
	ag-util.c

nodist_libaccounts_glib_la_SOURCES = \
	$(BUILT_SOURCES)

libaccounts_glib_includedir = $(includedir)/libaccounts-glib
libaccounts_glib_include_HEADERS = \
	accounts-glib.h \
	ag-account.h \
	ag-account-service.h \
	ag-application.h \
	ag-auth-data.h \
	ag-errors.h \
	ag-manager.h \
	ag-provider.h \
	ag-service.h \
	ag-service-type.h \
	ag-types.h

ag-marshal.h: ag-marshal.list Makefile
	$(AM_V_GEN)glib-genmarshal --header --prefix=ag_marshal $< > $@

ag-marshal.c: ag-marshal.list Makefile
	$(AM_V_GEN)glib-genmarshal --body --prefix=ag_marshal $< > $@

dist_noinst_DATA = ag-marshal.list

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = \
	ag-account.c \
	ag-account.h \
	ag-account-service.c \
	ag-account-service.h \
	ag-application.c \
	ag-application.h \
	ag-auth-data.c \
	ag-auth-data.h \
	ag-errors.h \
	ag-manager.c \
	ag-manager.h \
	ag-provider.c \
	ag-provider.h \
	ag-service.c \
	ag-service.h \
	ag-service-type.c \
	ag-service-type.h \
	ag-types.h

Accounts-1.0.gir: libaccounts-glib.la
Accounts_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
Accounts_1_0_gir_SCANNERFLAGS = \
	--identifier-prefix=Ag \
	--symbol-prefix=ag \
	--c-include="libaccounts-glib/accounts-glib.h" \
	--warn-all
Accounts_1_0_gir_CFLAGS = \
	$(LIBACCOUNTS_CFLAGS) \
	-I$(top_srcdir)
Accounts_1_0_gir_LIBS = libaccounts-glib.la
Accounts_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += Accounts-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

# XML DTDs
dtddir = $(datadir)/xml/accounts/schema/dtd
dist_dtd_DATA = \
	accounts-application.dtd \
	accounts-provider.dtd \
	accounts-service.dtd \
	accounts-service-type.dtd

# Vala bindings
vapidir = $(datadir)/vala/vapi
dist_vapi_DATA = \
	libaccounts-glib.deps \
	libaccounts-glib.vapi
