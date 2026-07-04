PREFIX ?= /usr
PYTHON_SOURCES=lib up-to-date validate-csv-data

build:

install:
	install -d $(DESTDIR)$(PREFIX)/share/distro-info
	install -m 644 debian.csv $(DESTDIR)$(PREFIX)/share/distro-info
	install -m 644 ubuntu.csv $(DESTDIR)$(PREFIX)/share/distro-info
	install -m 644 "elementary os.csv" $(DESTDIR)$(PREFIX)/share/distro-info

test:
	./validate-csv-data debian.csv
	./validate-csv-data devuan.csv
	./validate-csv-data elxr.csv
	./validate-csv-data ubuntu.csv

up-to-date:
	./up-to-date debian.csv
	./up-to-date devuan.csv
	./up-to-date elxr.csv
	./up-to-date ubuntu.csv

black:
	black -C $(PYTHON_SOURCES)

pylint:
	pylint $(PYTHON_SOURCES)

.PHONY: black build install pylint test up-to-date
