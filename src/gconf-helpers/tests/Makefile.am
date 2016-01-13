noinst_PROGRAMS = test-upgrade

test_upgrade_SOURCES = \
	test-upgrade.c \
	fake-keyring.h \
	fake-keyring.c \
	fake-gconf.h \
	fake-gconf.c

test_upgrade_CPPFLAGS = \
	-I $(top_srcdir)/src/utils \
	-I ${srcdir}/../ \
	-DTESTDIR=\"$(srcdir)\" \
	$(GTK_CFLAGS) \
	$(NMA_CFLAGS) \
	$(MIGRATION_CFLAGS)

test_upgrade_LDADD = \
	${builddir}/../libgconf-helpers-test.la \
	$(GTK_CFLAGS) \
	$(NMA_LIBS) \
	$(MIGRATION_LIBS)

check-local: test-upgrade
	$(abs_builddir)/test-upgrade

EXTRA_DIST = \
	test-import.xml \
	08wifi.xml \
	08vpnc.xml \
	08openvpn-not-saved.xml \
	08openvpn-saved.xml

