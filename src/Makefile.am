pkglibexec_PROGRAMS = indicator-bluetooth-service

indicator_bluetooth_service_SOURCES = \
	org-bluez.vala \
	bluetooth.vala \
	bluez.vala \
	desktop.vala \
	device.vala \
	main.vala \
	phone.vala \
	profile.vala \
	killswitch.vala \
	service.vala

indicator_bluetooth_service_VALAFLAGS = \
	--ccode \
	--vapidir=$(top_srcdir)/vapi/ \
	--vapidir=./ \
	--pkg config \
	--pkg rfkill \
	--pkg url-dispatcher \
	--pkg posix \
	--pkg glib-2.0 \
	--pkg gio-2.0

# -w to disable warnings for vala-generated code
indicator_bluetooth_service_CFLAGS = \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DLOCALE_DIR=\"$(datadir)/locale\" \
	-w \
	$(SERVICE_DEPS_CFLAGS)

indicator_bluetooth_service_LDADD = \
	$(SERVICE_DEPS_LIBS)

CLEANFILES = \
	$(patsubst %.vala,%.c,$(filter %.vala, $(SOURCES))) \
	*_vala.stamp
