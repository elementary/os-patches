# Copyright (c) 1999-2000, Marc-Andre Lemburg; mailto:mal@lemburg.com
# Copyright (c) 2000-2010, eGenix.com Software GmbH; mailto:info@egenix.com
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee or royalty is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that copyright notice and this permission notice appear in
# supporting documentation or portions thereof, including modifications,
# that you make.
#
# EGENIX.COM SOFTWARE GMBH DISCLAIMS ALL WARRANTIES WITH REGARD TO
# THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS, IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL,
# INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE !

import re

# Copied from CPython
# https://github.com/python/cpython/blob/v3.14.3/Lib/platform.py#L1414-L1438
def _parse_os_release(lines):
    # These fields are mandatory fields with well-known defaults
    # in practice all Linux distributions override NAME, ID, and PRETTY_NAME.
    info = {
        "NAME": "Linux",
        "ID": "linux",
        "PRETTY_NAME": "Linux",
    }

    # NAME=value with optional quotes (' or "). The regular expression is less
    # strict than shell lexer, but that's ok.
    os_release_line = re.compile(
        "^(?P<name>[a-zA-Z0-9_]+)=(?P<quote>[\"\']?)(?P<value>.*)(?P=quote)$"
    )
    # unescape five special characters mentioned in the standard
    os_release_unescape = re.compile(r"\\([\\\$\"\'`])")

    for line in lines:
        mo = os_release_line.match(line)
        if mo is not None:
            info[mo.group('name')] = os_release_unescape.sub(
                r"\1", mo.group('value')
            )

    return info
