# Looking in which build system we are
BUILD_SYSTEM := $(shell lsb_release --short --id)

all:
	# Setting BUILD_SYSTEM in the binary package
	sed -i -e 's/\(BUILD_SYSTEM="\).*"/\1'$(BUILD_SYSTEM)'"/g' casper.conf

	$(MAKE) -C casper-md5check
	set -e; \
	for x in bin/* scripts/casper scripts/casper-bottom/* \
	         ubiquity-hooks/*; do \
		sh -n $$x; \
	done

clean:
	$(MAKE) -C casper-md5check clean
