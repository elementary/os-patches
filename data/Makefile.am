noinst_PROGRAMS = indicator-keyboard-icon-generator

AM_CFLAGS = -w -DGNOME_DESKTOP_USE_UNSTABLE_API
AM_LDFLAGS = -lm
AM_VALAFLAGS = --enable-experimental-non-null   \
               --metadatadir $(top_srcdir)/deps \
               --vapidir $(top_srcdir)/deps

indicator_keyboard_icon_generator_SOURCES = main.vala                     \
                                            $(top_srcdir)/lib/common.vala
indicator_keyboard_icon_generator_VALAFLAGS = $(AM_VALAFLAGS)         \
                                              --pkg gee-1.0           \
                                              --pkg fontconfig        \
                                              --pkg freetype2         \
                                              --pkg pangoft2          \
                                              --pkg gtk+-3.0          \
                                              --pkg GDesktopEnums-3.0 \
                                              --pkg gnome-desktop-3.0 \
                                              --pkg Xkl-1.0
indicator_keyboard_icon_generator_CFLAGS = $(AM_CFLAGS)            \
                                           $(GEE_CFLAGS)           \
                                           $(PANGOFT2_CFLAGS)      \
                                           $(GTK_CFLAGS)           \
                                           $(GNOME_DESKTOP_CFLAGS) \
                                           $(LIBXKLAVIER_CFLAGS)   \
                                           $(COVERAGE_CFLAGS)
indicator_keyboard_icon_generator_LDFLAGS = $(AM_LDFLAGS)         \
                                            $(GEE_LIBS)           \
                                            $(PANGOFT2_LIBS)      \
                                            $(GTK_LIBS)           \
                                            $(GNOME_DESKTOP_LIBS) \
                                            $(LIBXKLAVIER_LIBS)   \
                                            $(COVERAGE_LDFLAGS)

dist_service_DATA = indicator-keyboard.service
servicedir = $(DBUS_SERVICE_DIR)

indicator-keyboard.service:
	$(AM_V_GEN) (echo '[D-BUS Service]';                                                     \
	             echo 'Name=com.canonical.indicator.keyboard';                               \
	             echo 'Exec=${libexecdir}/indicator-keyboard-service --use-gtk') > $@.tmp && \
	            mv $@.tmp $@

dist_indicator_DATA = com.canonical.indicator.keyboard
indicatordir = $(INDICATOR_DIR)

com.canonical.indicator.keyboard:
	$(AM_V_GEN) (echo '[Indicator Service]';                                              \
	             echo 'Name=indicator-keyboard';                                          \
	             echo 'ObjectPath=/com/canonical/indicator/keyboard';                     \
	             echo 'Position=80';                                                      \
	             echo '';                                                                 \
	             echo '[desktop]';                                                        \
	             echo 'ObjectPath=/com/canonical/indicator/keyboard/desktop';             \
	             echo '';                                                                 \
	             echo '[desktop_greeter]';                                                \
	             echo 'ObjectPath=/com/canonical/indicator/keyboard/desktop_greeter';     \
	             echo '';                                                                 \
	             echo '[desktop_lockscreen]';                                             \
	             echo 'ObjectPath=/com/canonical/indicator/keyboard/desktop_lockscreen';  \
	             echo '';                                                                 \
	             echo '[ubiquity]';                                                       \
	             echo 'ObjectPath=/com/canonical/indicator/keyboard/desktop') > $@.tmp && \
	            mv $@.tmp $@

gsettings_SCHEMAS = com.canonical.indicator.keyboard.gschema.xml

@GSETTINGS_RULES@

check_DATA = gschemas.compiled

gschemas.compiled: $(gsettings_SCHEMAS)
	$(GLIB_COMPILE_SCHEMAS) --strict .

EXTRA_DIST = $(dist_service_DATA)   \
	     $(dist_indicator_DATA) \
	     $(gsettings_SCHEMAS)
