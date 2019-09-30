
lib_LTLIBRARIES = libmessaging-menu.la

libmessaging_menu_ladir = $(includedir)/messaging-menu

libmessaging_menu_la_SOURCES = \
	messaging-menu-app.c \
	messaging-menu-message.c

libmessaging_menu_la_HEADERS = \
	messaging-menu-app.h \
	messaging-menu.h \
	messaging-menu-message.h

libmessaging_menu_la_LIBADD = \
	$(GIO_LIBS) \
	$(top_builddir)/common/libmessaging-common.la

libmessaging_menu_la_CFLAGS = \
	-I$(top_builddir)/common \
	$(GIO_CFLAGS) \
	-Wall

libmessaging_menu_la_LDFLAGS = -export-symbols-regex "^messaging_menu_.*"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = messaging-menu.pc


-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION

MessagingMenu-1.0.gir: libmessaging-menu.la
MessagingMenu_1_0_gir_NAMESPACE = MessagingMenu
MessagingMenu_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
MessagingMenu_1_0_gir_CFLAGS = $(INCLUDES) $(GIO_CFLAGS)
MessagingMenu_1_0_gir_SCANNERFLAGS = --c-include="messaging-menu.h"
MessagingMenu_1_0_gir_LIBS = libmessaging-menu.la
MessagingMenu_1_0_gir_FILES = \
	messaging-menu-app.c \
	messaging-menu-app.h \
	messaging-menu-message.c \
	messaging-menu-message.h
MessagingMenu_1_0_gir_EXPORT_PACKAGES = messaging-menu
INTROSPECTION_GIRS += MessagingMenu-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)


#########################
# VAPI Files
#########################

if HAVE_VALA

vapidir = $(datadir)/vala/vapi
vapi_DATA = MessagingMenu-1.0.vapi

MessagingMenu-1.0.vapi: MessagingMenu-1.0.gir
	$(VALA_API_GEN) --pkg gio-2.0 --library=MessagingMenu-1.0 $<

CLEANFILES += $(vapi_DATA)

endif

endif
