#!/usr/bin/python3
#
# Copyright (C) Canonical Ltd
#
# SPDX-License-Identifier: GPL-2.0+

"""deb822 parser with support for comment headers and footers."""

import collections
import io
import typing

import apt_pkg

T = typing.TypeVar("T")


class Section:
    """A single deb822 section, possibly with comments.

    This represents a single deb822 section.
    """

    tags: collections.OrderedDict[str, str]
    _case_mapping: dict[str, str]
    header: str
    footer: str

    def __init__(self, section: typing.Union[str, "Section"]):
        if isinstance(section, Section):
            self.tags = collections.OrderedDict(section.tags)
            self._case_mapping = {k.casefold(): k for k in self.tags}
            self.header = section.header
            self.footer = section.footer
            return

        comments = ["", ""]
        in_section = False
        trimmed_section = ""

        for line in section.split("\n"):
            if line.startswith("#"):
                # remove the leading #
                line = line[1:]
                comments[in_section] += line + "\n"
                continue

            in_section = True
            trimmed_section += line + "\n"

        self.tags = collections.OrderedDict(apt_pkg.TagSection(trimmed_section))
        self._case_mapping = {k.casefold(): k for k in self.tags}
        self.header, self.footer = comments

    def __getitem__(self, key: str) -> str:
        """Get the value of a field."""
        return self.tags[self._case_mapping.get(key.casefold(), key)]

    def __delitem__(self, key: str) -> None:
        """Delete a field"""
        del self.tags[self._case_mapping.get(key.casefold(), key)]

    def __setitem__(self, key: str, val: str) -> None:
        """Set the value of a field."""
        if key.casefold() not in self._case_mapping:
            self._case_mapping[key.casefold()] = key
        self.tags[self._case_mapping[key.casefold()]] = val

    def __bool__(self) -> bool:
        return bool(self.tags)

    @typing.overload
    def get(self, key: str) -> str | None:
        ...

    @typing.overload
    def get(self, key: str, default: T) -> T | str:
        ...

    def get(self, key: str, default: T | None = None) -> T | None | str:
        try:
            return self[key]
        except KeyError:
            return default

    @staticmethod
    def __comment_lines(content: str) -> str:
        return (
            "\n".join("#" + line for line in content.splitlines()) + "\n"
            if content
            else ""
        )

    def __str__(self) -> str:
        """Canonical string rendering of this section."""
        return (
            self.__comment_lines(self.header)
            + "".join(f"{k}: {v}\n" for k, v in self.tags.items())
            + self.__comment_lines(self.footer)
        )


class File:
    """
    Parse a given file object into a list of Section objects.
    """

    def __init__(self, fobj: io.TextIOBase):
        self.sections = []
        section = ""
        for line in fobj:
            if not line.isspace():
                # A line is part of the section if it has non-whitespace characters
                section += line
            elif section:
                # Our line is just whitespace and we have gathered section content, so let's write out the section
                self.sections.append(Section(section))
                section = ""

        # The final section may not be terminated by an empty line
        if section:
            self.sections.append(Section(section))

    def __iter__(self) -> typing.Iterator[Section]:
        return iter(self.sections)

    def __str__(self) -> str:
        return "\n".join(str(s) for s in self.sections)


if __name__ == "__main__":
    st = """# Header
# More header
K1: V1
# Inline
K2: V2
 # not a comment
# Footer
# More footer
"""

    s = Section(st)

    print(s)
