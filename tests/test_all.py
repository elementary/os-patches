#!/usr/bin/python3
# Copyright (C) 2009 Julian Andres Klode <jak@debian.org>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Run all available unit tests."""
import os
import sys
import unittest
import unittest.runner

# Python 3 only provides abiflags since 3.2
if not hasattr(sys, "pydebug"):
    if sys.abiflags.startswith("d"):
        sys.pydebug = True
    else:
        sys.pydebug = False


def get_library_dir():
    # Find the path to the built apt_pkg and apt_inst extensions
    if not os.path.exists("../build"):
        return None
    from sysconfig import get_platform, get_python_version

    # Set the path to the build directory.
    plat_specifier = f".{get_platform()}-{get_python_version()}"
    library_dir = "../build/lib{}{}".format(
        plat_specifier,
        (sys.pydebug and "-pydebug" or ""),
    )
    return os.path.abspath(library_dir)


class MyTestRunner(unittest.runner.TextTestRunner):
    def __init__(self, *args, **kwargs):
        kwargs["stream"] = sys.stdout
        super().__init__(*args, **kwargs)


if __name__ == "__main__":
    if not os.access("/etc/apt/sources.list", os.R_OK):
        sys.stdout.write("[tests] Skipping because sources.list is not readable\n")
        sys.exit(0)

    sys.stdout.write("[tests] Running on %s\n" % sys.version.replace("\n", ""))
    dirname = os.path.dirname(__file__)
    if dirname:
        os.chdir(dirname)
    library_dir = get_library_dir()
    if "pybuild" in os.getenv("PYTHONPATH", ""):
        # pybuild already supplied us with a path to check for
        sys.stdout.write("Using pybuild supplied build dir\n")
    elif library_dir:
        sys.stdout.write("Using library_dir: '%s'\n" % library_dir)
        sys.path.insert(0, os.path.abspath(library_dir))

    for path in os.listdir("."):
        if path.endswith(".py") and os.path.isfile(path) and path.startswith("test_"):
            exec("from %s import *" % path[:-3])

    unittest.main(testRunner=MyTestRunner)
