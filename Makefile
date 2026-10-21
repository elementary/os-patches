# Use YYYY.0M.0D defined in https://calver.org/
VERSION = 2026.10.12

NAME = distro-info-data
PREFIX ?= /usr
SCRIPTS = up-to-date validate-csv-data
PYTHON_SOURCES= lib $(SCRIPTS)
SOURCES = $(wildcard lib/*.py) $(SCRIPTS) $(wildcard *.csv) Makefile README.md .gitignore .gitlab-ci.yml

build:

install:
	install -d $(DESTDIR)$(PREFIX)/share/distro-info
	install -m 644 $(wildcard *.csv) $(DESTDIR)$(PREFIX)/share/distro-info

clean:
	rm -rf .mypy_cache/ lib/__pycache__/

test:
	./validate-csv-data $(DATADIR)debian.csv
	./validate-csv-data $(DATADIR)devuan.csv
	./validate-csv-data $(DATADIR)elxr.csv
	./validate-csv-data $(DATADIR)ubuntu.csv

up-to-date:
	./up-to-date $(DATADIR)debian.csv
	./up-to-date $(DATADIR)devuan.csv
	./up-to-date $(DATADIR)elxr.csv
	./up-to-date $(DATADIR)ubuntu.csv

lint: isort black mypy pylint

black:
	black -C --check --diff $(PYTHON_SOURCES)

isort:
	isort --check-only --diff $(PYTHON_SOURCES)

mypy:
	mypy --scripts-are-modules $(PYTHON_SOURCES)

pylint:
	pylint $(PYTHON_SOURCES)

%.asc: %
	gpg --armor --batch --detach-sign --yes --output $@ $^

%.tar.xz: $(SOURCES)
	tar -cJf $@ --transform 's,^,$(NAME)-$(VERSION)/,' --owner=0 --group=0 $^

dist: ../$(NAME)-$(VERSION).tar.xz ../$(NAME)-$(VERSION).tar.xz.asc

../$(NAME)_$(VERSION).orig.%: ../$(NAME)-$(VERSION).%
	ln -sf $(notdir $^) $@

debian-dist: dist ../$(NAME)_$(VERSION).orig.tar.xz ../$(NAME)_$(VERSION).orig.tar.xz.asc

.PHONY: black build clean debian-dist dist install isort lint mypy pylint test up-to-date
