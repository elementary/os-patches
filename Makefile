PREFIX ?= /usr
PYTHON_SOURCES=lib up-to-date validate-csv-data

build:

install:
	install -d $(DESTDIR)$(PREFIX)/share/distro-info
	install -m 644 $(wildcard *.csv) $(DESTDIR)$(PREFIX)/share/distro-info

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

lint: isort black mypy pylint

black:
	black -C --check --diff $(PYTHON_SOURCES)

isort:
	isort --check-only --diff $(PYTHON_SOURCES)

mypy:
	mypy --scripts-are-modules $(PYTHON_SOURCES)

pylint:
	pylint $(PYTHON_SOURCES)

.PHONY: black build install isort lint mypy pylint test up-to-date
