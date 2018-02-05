#!/usr/bin/python3
# -*- Mode: Python; indent-tabs-mode: nil; tab-width: 4; coding: utf-8 -*-

import os
import subprocess
import unittest

import testcommon


class TestPyflakesClean(testcommon.TestCase):

    EXCLUDES = ["build", "tests/old"]
    TOPLEVEL = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

    def is_excluded_path(self, path):
        for exclude in self.EXCLUDES:
            if path.startswith(os.path.join(self.TOPLEVEL, exclude)):
                return True
        return False

    def get_py_files(self, toplevel):
        files = []
        for path, dirnames, filenames in os.walk(self.TOPLEVEL):
            if self.is_excluded_path(path):
                continue
            for filename in filenames:
                if os.path.splitext(filename)[1] == ".py":
                    files.append(os.path.join(path, filename))
        return files

    def test_pyflakes_clean(self):
        cmd = ["pyflakes"] + self.get_py_files(self.TOPLEVEL)
        res = subprocess.call(cmd)
        if res != 0:
            self.fail("pyflakes failed with: %s" % res)


if __name__ == "__main__":
    import logging
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
