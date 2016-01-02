XSLTPROC_FLAGS = \
        --nonet \
        --stringparam man.output.quietly 1 \
        --stringparam funcsynopsis.style ansi \
        --stringparam man.th.extra1.suppress 1 \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0

.xml.1:
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

.xml.5:
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

man_MANS = \
	xdg-user-dir.1 \
	xdg-user-dirs-update.1 \
	user-dirs.conf.5 \
	user-dirs.defaults.5 \
	user-dirs.dirs.5

xml_files = $(patsubst %.5,%.xml,$(patsubst %.1,%.xml,$(man_MANS)))

EXTRA_DIST = $(xml_files)

DISTCLEANFILES = $(man_MANS)
