#!/usr/bin/python3
#
# Copyright (C) 2019 Colomban Wendling <cwendling@hypra.fr>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Helper checking argv[1] is a valid, writable, file descriptor"""

import os
from sys import argv

assert len(argv) == 2
with os.fdopen(int(argv[1]), "w") as f:
    assert f.write("test") == 4
