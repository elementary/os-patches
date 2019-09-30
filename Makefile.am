SUBDIRS = libaccounts-glib docs tools
if ENABLE_PYTHON
SUBDIRS += pygobject
endif
if ENABLE_TESTS
SUBDIRS += tests
endif

include $(top_srcdir)/Makefile.am.coverage

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-introspection=yes \
	--enable-man

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_in_files = libaccounts-glib.pc.in
pkgconfig_DATA = $(pkgconfig_in_files:.pc.in=.pc)

backupconfdir = ${prefix}/share/backup-framework/applications
backupconf_in_files = accounts.conf.in 
backupconf_DATA = $(backupconf_in_files:.conf.in=.conf)

dist-hook: git-changelog-hook

git-changelog-hook: Makefile
	$(AM_V_at)if $(top_srcdir)/build-aux/missing --run git \
		--git-dir=$(top_srcdir)/.git --work-tree=$(top_srcdir) log \
		--no-merges --date=short --pretty='tformat:%cd  %an  <%ae>%n%n%s%n%n%b' | \
		$(SED) -e '/[^  ]/,/^[  ]*$$/ !d' > .ChangeLog.tmp; \
	then mv -f .ChangeLog.tmp "$(top_distdir)/ChangeLog"; \
	else rm -f .ChangeLog.tmp; exit 1; fi

CLEANFILES = \
	$(backupconf_DATA)

DISTCLEANFILES = \
	$(pkgconfig_DATA)

dist_noinst_DATA = \
	$(backupconf_in_files) \
	m4/introspection.m4

$(backupconf_DATA): $(backupconf_in_files) 
	$(AM_V_GEN)$(SED) -e "s|\@DATABASE_DIR\@|$(DATABASE_DIR)|" $< > $@

.PHONY:  git-changelog-hook
