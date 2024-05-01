#  distinfo.py - provide meta information for distro repositories
#
#  Copyright (c) 2005 Gustavo Noronha Silva <kov@debian.org>
#  Copyright (c) 2006-2007 Sebastian Heinlein <glatzor@ubuntu.com>
#
#  Authors: Gustavo Noronha Silva <kov@debian.org>
#           Sebastian Heinlein <glatzor@ubuntu.com>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
#  USA

import csv
import errno
import logging
import os
import re
from collections.abc import Iterator
from subprocess import PIPE, Popen
from typing import cast

import apt_pkg
from apt_pkg import gettext as _


def _expand_template(template: str, csv_path: str) -> Iterator[str]:
    """Expand the given template.

    A template file consists of a header, followed by paragraphs
    of templated suites, followed by a footer. A templated suite
    is any paragraph where the Suite field contains {.

    This function expands all templated suites using the information
    found in the CSV file supplied by distro-info-data.

    It yields lines of template info.
    """

    known_suites = set()

    # Copy out any header, and gather all hardcoded suites
    with apt_pkg.TagFile(template) as tmpl:
        for section in tmpl:
            if "X-Exclude-Suites" in section:
                known_suites.update(section["X-Exclude-Suites"].split(", "))
            if "Suite" in section:
                if "{" in section["Suite"]:
                    break

                known_suites.add(section["Suite"])

            yield from str(section).splitlines()
        else:
            # We did not break, so we did copy all of them
            return

        for section in tmpl:
            if "Suite" in section:
                known_suites.add(section["Suite"])

    with open(csv_path) as csv_object:
        releases = reversed(list(csv.DictReader(csv_object)))

    # Perform template substitution on the middle of the list
    for rel in releases:
        if rel["series"] in known_suites:
            continue
        yield ""
        rel["version"] = rel["version"].replace(" LTS", "")
        with apt_pkg.TagFile(template) as tmpl:
            for section in tmpl:
                # Only work on template sections, this skips head and tails
                if "Suite" not in section or "{" not in section["Suite"]:
                    continue
                if "X-Version" in section:
                    # Version requirements. Maybe should be made nicer
                    ver = rel["version"]
                    if any(
                        (
                            field.startswith("le")
                            and apt_pkg.version_compare(field[3:], ver) < 0
                        )
                        or (
                            field.startswith("ge")
                            and apt_pkg.version_compare(field[3:], ver) > 0
                        )
                        for field in section["X-Version"].split(", ")
                    ):
                        continue

                for line in str(section).format(**rel).splitlines():
                    if line.startswith("X-Version"):
                        continue
                    yield line

    # Copy out remaining suites
    with apt_pkg.TagFile(template) as tmpl:
        # Skip the head again, we don't want to copy it twice
        for section in tmpl:
            if "Suite" in section and "{" in section["Suite"]:
                break

        for section in tmpl:
            # Ignore any template parts and copy the rest out,
            # this is the inverse of the template substitution loop
            if "Suite" in section and "{" in section["Suite"]:
                continue

            yield from str(section).splitlines()


class Template:
    def __init__(self) -> None:
        self.name: str | None = None
        self.child = False
        self.parents: list[Template] = []  # ref to parent template(s)
        self.match_name: str | None = None
        self.description: str | None = None
        self.base_uri: str | None = None
        self.type: str | None = None
        self.components: list[Component] = []
        self.children: list[Template] = []
        self.match_uri: str | None = None
        self.mirror_set: dict[str, Mirror] = {}
        self.distribution: str | None = None
        self.available = True
        self.official = True

    def has_component(self, comp: str) -> bool:
        """Check if the distribution provides the given component"""
        return comp in (c.name for c in self.components)

    def is_mirror(self, url: str) -> bool:
        """Check if a given url of a repository is a valid mirror"""
        proto, hostname, dir = split_url(url)
        if hostname in self.mirror_set:
            return self.mirror_set[hostname].has_repository(proto, dir)
        else:
            return False


class Component:
    def __init__(
        self,
        name: str,
        desc: str | None = None,
        long_desc: str | None = None,
        parent_component: str | None = None,
    ):
        self.name = name
        self.description = desc
        self.description_long = long_desc
        self.parent_component = parent_component

    def get_parent_component(self) -> str | None:
        return self.parent_component

    def set_parent_component(self, parent: str) -> None:
        self.parent_component = parent

    def get_description(self) -> str | None:
        if self.description_long is not None:
            return self.description_long
        elif self.description is not None:
            return self.description
        else:
            return None

    def set_description(self, desc: str) -> None:
        self.description = desc

    def set_description_long(self, desc: str) -> None:
        self.description_long = desc

    def get_description_long(self) -> str | None:
        return self.description_long


class Mirror:
    """Storage for mirror related information"""

    def __init__(
        self, proto: str, hostname: str, dir: str, location: str | None = None
    ):
        self.hostname = hostname
        self.repositories: list[Repository] = []
        self.add_repository(proto, dir)
        self.location = location

    def add_repository(self, proto: str, dir: str) -> None:
        self.repositories.append(Repository(proto, dir))

    def get_repositories_for_proto(self, proto: str) -> list["Repository"]:
        return [r for r in self.repositories if r.proto == proto]

    def has_repository(self, proto: str, dir: str) -> bool:
        if dir is None:
            return False
        for r in self.repositories:
            if r.proto == proto and dir in r.dir:
                return True
        return False

    def get_repo_urls(self) -> list[str]:
        return [r.get_url(self.hostname) for r in self.repositories]

    def get_location(self) -> str | None:
        return self.location

    def set_location(self, location: str) -> None:
        self.location = location


class Repository:
    def __init__(self, proto: str, dir: str) -> None:
        self.proto = proto
        self.dir = dir

    def get_info(self) -> tuple[str, str]:
        return self.proto, self.dir

    def get_url(self, hostname: str) -> str:
        return f"{self.proto}://{hostname}/{self.dir}"


def split_url(url: str) -> list[str]:
    """split a given URL into the protocoll, the hostname and the dir part"""
    split = re.split(":*\\/+", url, maxsplit=2)
    while len(split) < 3:
        split.append(None)
    return split


class DistInfo:
    def __init__(
        self,
        dist: str | None = None,
        base_dir: str = "/usr/share/python-apt/templates",
    ):
        self.metarelease_uri = ""
        self.templates: list[Template] = []
        self.arch = apt_pkg.config.find("APT::Architecture")

        location = None
        match_loc = re.compile(r"^#LOC:(.+)$")
        match_mirror_line = re.compile(
            r"^(#LOC:.+)|(((http)|(ftp)|(rsync)|(file)|(mirror)|(https))://"
            r"[A-Za-z0-9/\.:\-_@]+)$"
        )
        # match_mirror_line = re.compile(r".+")

        if not dist:
            try:
                dist = (
                    Popen(
                        ["lsb_release", "-i", "-s"],
                        universal_newlines=True,
                        stdout=PIPE,
                    )
                    .communicate()[0]
                    .strip()
                )
            except OSError as exc:
                if exc.errno != errno.ENOENT:
                    logging.warning("lsb_release failed, using defaults: %s" % exc)
                dist = "Debian"

        self.dist = dist

        map_mirror_sets = {}

        dist_fname = f"{base_dir}/{dist}.info"
        csv_fname = f"/usr/share/distro-info/{dist.lower()}.csv"

        # FIXME: Logic doesn't work with types.
        template = cast(Template, None)
        component = cast(Component, None)
        for line in _expand_template(dist_fname, csv_fname):
            tokens = line.split(":", 1)
            if len(tokens) < 2:
                continue
            field = tokens[0].strip()
            value = tokens[1].strip()
            if field == "ChangelogURI":
                self.changelogs_uri = _(value)
            elif field == "MetaReleaseURI":
                self.metarelease_uri = value
            elif field == "Suite":
                self.finish_template(template, component)
                component = cast(Component, None)  # FIXME
                template = Template()
                template.name = value
                template.distribution = dist
                template.match_name = "^%s$" % value
            elif field == "MatchName":
                template.match_name = value
            elif field == "ParentSuite":
                template.child = True
                for nanny in self.templates:
                    # look for parent and add back ref to it
                    if nanny.name == value:
                        template.parents.append(nanny)
                        nanny.children.append(template)
            elif field == "Available":
                template.available = apt_pkg.string_to_bool(value)
            elif field == "Official":
                template.official = apt_pkg.string_to_bool(value)
            elif field == "RepositoryType":
                template.type = value
            elif field == "BaseURI" and not template.base_uri:
                template.base_uri = value
            elif field == "BaseURI-%s" % self.arch:
                template.base_uri = value
            elif field == "MatchURI" and not template.match_uri:
                template.match_uri = value
            elif field == "MatchURI-%s" % self.arch:
                template.match_uri = value
            elif field == "MirrorsFile" or field == "MirrorsFile-%s" % self.arch:
                # Make the path absolute.
                value = (
                    os.path.isabs(value)
                    and value
                    or os.path.abspath(os.path.join(base_dir, value))
                )
                if value not in map_mirror_sets:
                    mirror_set: dict[str, Mirror] = {}
                    try:
                        with open(value) as value_f:
                            mirror_data = list(
                                filter(
                                    match_mirror_line.match,
                                    [x.strip() for x in value_f],
                                )
                            )
                    except Exception:
                        print(f"WARNING: Failed to read mirror file {value}")
                        mirror_data = []
                    for line in mirror_data:
                        if line.startswith("#LOC:"):
                            location = match_loc.sub(r"\1", line)
                            continue
                        (proto, hostname, dir) = split_url(line)
                        if hostname in mirror_set:
                            mirror_set[hostname].add_repository(proto, dir)
                        else:
                            mirror_set[hostname] = Mirror(
                                proto, hostname, dir, location
                            )
                    map_mirror_sets[value] = mirror_set
                template.mirror_set = map_mirror_sets[value]
            elif field == "Description":
                template.description = _(value)
            elif field == "Component":
                if component and not template.has_component(component.name):
                    template.components.append(component)
                component = Component(value)
            elif field == "CompDescription":
                component.set_description(_(value))
            elif field == "CompDescriptionLong":
                component.set_description_long(_(value))
            elif field == "ParentComponent":
                component.set_parent_component(value)
        self.finish_template(template, component)
        template = cast(Template, None)
        component = cast(Component, None)

    def finish_template(self, template: Template, component: Component | None) -> None:
        "finish the current tempalte"
        if not template:
            return
        # reuse some properties of the parent template
        if template.match_uri is None and template.child:
            for t in template.parents:
                if t.match_uri:
                    template.match_uri = t.match_uri
                    break
        if template.mirror_set == {} and template.child:
            for t in template.parents:
                if t.match_uri:
                    template.mirror_set = t.mirror_set
                    break
        if component and not template.has_component(component.name):
            template.components.append(component)
            component = None
        # the official attribute is inherited
        for t in template.parents:
            template.official = t.official
        self.templates.append(template)


if __name__ == "__main__":
    d = DistInfo("Ubuntu", "/usr/share/python-apt/templates")
    logging.info(d.changelogs_uri)
    for template in d.templates:
        logging.info("\nSuite: %s" % template.name)
        logging.info("Desc: %s" % template.description)
        logging.info("BaseURI: %s" % template.base_uri)
        logging.info("MatchURI: %s" % template.match_uri)
        if template.mirror_set != {}:
            logging.info("Mirrors: %s" % list(template.mirror_set.keys()))
        for comp in template.components:
            logging.info(f" {comp.name} -{comp.description} -{comp.description_long}")
        for child in template.children:
            logging.info("  %s" % child.description)
