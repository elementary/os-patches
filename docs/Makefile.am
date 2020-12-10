XSLTPROC_FLAGS = \
        --nonet \
        --stringparam man.output.quietly 1 \
        --stringparam funcsynopsis.style ansi \
        --stringparam man.th.extra1.suppress 1 \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0


plymouth.1: plymouth1.xml
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

%.8: %.xml
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
	
%.1: %.xml
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<
	
man_MANS = \
	plymouth.1 \
	plymouth.8 \
	plymouthd.8 \
	plymouth-set-default-theme.1

EXTRA_DIST = \
	plymouth.xml \
	plymouth1.xml \
	plymouth-set-default-theme.xml \
	plymouthd.xml

CLEANFILES = $(man_MANS)
