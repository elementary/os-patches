#!/usr/bin/python3
#
# Copyright (C) 2019 Colomban Wendling <cwendling@hypra.fr>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying InstallProgress works when spawning dpkg directly
(i.e. when installing standalone .debs)."""
import os
import sys
import unittest

from test_all import get_library_dir

libdir = get_library_dir()
if libdir:
    sys.path.insert(0, libdir)
import testcommon

from apt.progress.base import InstallProgress


class RunHelper:
    def __init__(self):
        self.script = "helper_install_progress_run.py"

    def do_install(self, fd):
        return os.spawnl(os.P_WAIT, self.script, self.script, str(fd))


class TestInstallProgressExec(testcommon.TestCase):
    """test that InstallProgress.run() passes a valid file descriptor to
    a child process"""

    def test_run(self):
        with InstallProgress() as prog:
            self.assertEqual(prog.run(RunHelper()), 0)


if __name__ == "__main__":
    os.chdir(os.path.dirname(__file__))
    unittest.main()
