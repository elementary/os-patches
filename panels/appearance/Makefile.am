# This is used in PANEL_CFLAGS
cappletname = appearance

uidir = $(pkgdatadir)/ui/appearance/
dist_ui_DATA = \
	appearance.ui \
	display-base.png \
	display-overlay.png

slideshowicondir = $(datadir)/icons/hicolor/scalable/categories/
dist_slideshowicon_DATA = ucc-slideshow-symbolic.svg

slideshowemblemdir = $(datadir)/icons/hicolor/scalable/emblems/
dist_slideshowemblem_DATA = ucc-slideshow-emblem.svg

INCLUDES =						\
	$(PANEL_CFLAGS)					\
	$(APPEARANCE_PANEL_CFLAGS) \
	-DG_LOG_DOMAIN="\"$(PACKAGE)\"" \
	-DLOCALEDIR="\"$(datadir)/locale\"" \
	-DPKGDATADIR="\"$(uidir)\"" \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libappearance.la

BUILT_SOURCES = \
	gdesktop-enums-types.c \
	gdesktop-enums-types.h

libappearance_la_SOURCES = \
	$(BUILT_SOURCES) \
	background-module.c \
	cc-appearance-panel.c \
	cc-appearance-panel.h \
	cc-appearance-item.c \
	cc-appearance-item.h \
	cc-appearance-xml.c \
	cc-appearance-xml.h \
	bg-source.c \
	bg-source.h \
	bg-pictures-source.c \
	bg-pictures-source.h \
	bg-wallpapers-source.c \
	bg-wallpapers-source.h \
	bg-colors-source.c \
	bg-colors-source.h

libappearance_la_LIBADD = $(PANEL_LIBS) $(APPEARANCE_PANEL_LIBS)
libappearance_la_LDFLAGS = $(PANEL_LDFLAGS)

if WITH_LIBSOCIALWEB
libappearance_la_SOURCES += bg-flickr-source.c bg-flickr-source.h
INCLUDES += $(SOCIALWEB_CFLAGS)
libappearance_la_LIBADD += $(SOCIALWEB_LIBS)
endif

gdesktop-enums-types.h: stamp-gdesktop-enums-types.h
	@true

stamp-gdesktop-enums-types.h: $(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-appearance-item.h Makefile
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) \
	  --fhead "#ifndef __GDESKTOP_ENUMS_TYPES_H__\n#define __GDESKTOP_ENUMS_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	  --fprod "/* enumerations from \"@filename@\" */\n" \
	  --vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define G_DESKTOP_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
	  --ftail "G_END_DECLS\n\n#endif /* __GDESKTOP_ENUMS_TYPES_H__ */" $(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-appearance-item.h) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh gdesktop-enums-types.h || cp xgen-gtbh gdesktop-enums-types.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)

gdesktop-enums-types.c: $(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-appearance-item.h Makefile gdesktop-enums-types.h
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) \
	  --fhead "#include <gdesktop-enums.h>\n#include \"gdesktop-enums-types.h\"\n#include \"cc-appearance-item.h\"" \
	  --fprod "\n/* enumerations from \"@filename@\" */" \
	  --vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
	  --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
	  --vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	  $(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-appearance-item.h) > xgen-gtbc \
	&& cp xgen-gtbc gdesktop-enums-types.c  \
	&& rm -f xgen-gtbc

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = unity-appearance-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES) stamp-gdesktop-enums-types.h

-include $(top_srcdir)/git.mk
