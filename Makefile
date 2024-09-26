PREFIX ?= /usr
PYTHON_SOURCES=lib up-to-date validate-csv-data

build:

install:
	install -d $(DESTDIR)$(PREFIX)/share/distro-info
	install -m 644 $(wildcard *.csv) $(DESTDIR)$(PREFIX)/share/distro-info

test:
	./validate-csv-data -d debian.csv
	./validate-csv-data -u ubuntu.csv

up-to-date:
	./up-to-date -d debian.csv
	./up-to-date -u ubuntu.csv

black:
	black -C $(PYTHON_SOURCES)

pylint:
	pylint $(PYTHON_SOURCES)

.PHONY: black build install pylint test up-to-date
