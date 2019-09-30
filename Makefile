PROGRAM	=	im-config
ifeq ($(wildcard debian/changelog),)
VERSION = VCS-$(shell date -u  +%Y%m%d%H%M)
else
VERSION = $(shell dpkg-parsechangelog --format dpkg|\
		sed -ne '/^Version/s/Version: *//p')
endif

# escape  with \#
LANGS = $(shell grep -v '^\#' po/LINGUAS)
POTFILESIN = $(shell grep -v '^\#' po/POTFILES.in)

all: share/xinputrc.common mo

share/xinputrc.common: share/xinputrc.common.in
	sed -e "s/@@VERSION@@/$(VERSION)/" <$< >$@

pot: po/$(PROGRAM).pot

po/$(PROGRAM).pot:
	xgettext -o $@ --language=Shell $(PROGRAM) $(POTFILESIN)

po/%.po: po/$(PROGRAM).pot
	msgmerge -U $@ $<

po/locale/%/LC_MESSAGES/$(PROGRAM).mo: po/%.po
	mkdir -p po/locale/$*/LC_MESSAGES
	msgfmt -o $@ $<

mo:	$(addsuffix /LC_MESSAGES/$(PROGRAM).mo, $(addprefix po/locale/, $(LANGS)))

po:	$(addsuffix .po, $(addprefix po/, $(LANGS)))

foo:
	echo "$(addsuffix /LC_MESSAGES/$(PROGRAM).mo, $(addprefix po/locale/, $(LANGS)))"
	echo "-----"
	echo "$(addsuffix .po, $(addprefix po/, $(LANGS)))"

clean:
	-rm -f share/xinputrc.common
	rm -rf po/locale
	rm -f  po/*.po~ po/*.mo

distclean: clean

update:
	rm -f po/$(PROGRAM).pot
	$(MAKE) po
	$(MAKE) clean

.PHONY: all pot distclean clean mo update po
