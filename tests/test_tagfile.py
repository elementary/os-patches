#!/usr/bin/python3
#
# Copyright (C) 2010 Michael Vogt <mvo@ubuntu.com>
# Copyright (C) 2012 Canonical Ltd.
# Author: Colin Watson <cjwatson@ubuntu.com>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of apt_pkg.TagFile"""

import glob
import os
import shutil
import sys
import tempfile
import unittest

from test_all import get_library_dir

libdir = get_library_dir()
if libdir:
    sys.path.insert(0, libdir)

import apt_pkg
import testcommon


class TestOpenMaybeClearSigned(testcommon.TestCase):
    def test_open_trivial(self):
        basepath = os.path.dirname(__file__)
        fd = apt_pkg.open_maybe_clear_signed_file(
            os.path.join(basepath, "./data/test_debs/hello_2.5-1.dsc")
        )
        with os.fdopen(fd) as f:
            data = f.read()
        self.assertTrue(data.startswith("Format: 1.0\n"))

    def test_open_normal(self):
        basepath = os.path.dirname(__file__)
        fd = apt_pkg.open_maybe_clear_signed_file(
            os.path.join(basepath, "./data/misc/foo_Release")
        )
        with os.fdopen(fd) as f:
            data = f.read()
        self.assertTrue(data.startswith("Origin: Ubuntu\n"))

    def xtest_open_does_not_exit(self):
        with self.assertRaises(SystemError):
            apt_pkg.open_maybe_clear_signed_file("does-not-exists")


class TestTagFile(testcommon.TestCase):
    """test the apt_pkg.TagFile"""

    def setUp(self):
        testcommon.TestCase.setUp(self)
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def test_tag_file(self):
        basepath = os.path.dirname(__file__)
        tagfilepath = os.path.join(basepath, "./data/tagfile/*")
        # test once for compressed and uncompressed
        for testfile in glob.glob(tagfilepath):
            # test once using the open() method and once using the path
            tagfile = apt_pkg.TagFile(testfile)
            for i, stanza in enumerate(tagfile):
                pass
            self.assertEqual(i, 2)
            with open(testfile) as f:
                tagfile = apt_pkg.TagFile(f)
                for i, stanza in enumerate(tagfile):
                    pass
                self.assertEqual(i, 2)

    def test_errors(self):
        # Raises SystemError via lbiapt
        self.assertRaises(SystemError, apt_pkg.TagFile, "not-there-no-no")
        # Raises Type error
        self.assertRaises(TypeError, apt_pkg.TagFile, object())

    def test_utf8(self):
        value = "Tést Persön <test@example.org>"
        packages = os.path.join(self.temp_dir, "Packages")
        with open(packages, "w", encoding="UTF-8") as packages_file:
            print("Maintainer: %s" % value, file=packages_file)
            print("", file=packages_file)
        with open(packages, encoding="UTF-8") as packages_file:
            tagfile = apt_pkg.TagFile(packages_file)
            tagfile.step()
            self.assertEqual(value, tagfile.section["Maintainer"])

    def test_latin1(self):
        value = "Tést Persön <test@example.org>"
        packages = os.path.join(self.temp_dir, "Packages")
        with open(packages, "w", encoding="ISO-8859-1") as packages_file:
            print("Maintainer: %s" % value, file=packages_file)
            print("", file=packages_file)
        with open(packages) as packages_file:
            tagfile = apt_pkg.TagFile(packages_file, bytes=True)
            tagfile.step()
            self.assertEqual(value.encode("ISO-8859-1"), tagfile.section["Maintainer"])
        with open(packages, encoding="ISO-8859-1") as packages_file:
            tagfile = apt_pkg.TagFile(packages_file)
            tagfile.step()
            self.assertEqual(value, tagfile.section["Maintainer"])

    def test_mixed(self):
        value = "Tést Persön <test@example.org>"
        packages = os.path.join(self.temp_dir, "Packages")
        with open(packages, "w", encoding="UTF-8") as packages_file:
            print("Maintainer: %s" % value, file=packages_file)
            print("", file=packages_file)
        with open(packages, "a", encoding="ISO-8859-1") as packages_file:
            print("Maintainer: %s" % value, file=packages_file)
            print("", file=packages_file)
        with open(packages) as packages_file:
            tagfile = apt_pkg.TagFile(packages_file, bytes=True)
            tagfile.step()
            self.assertEqual(value.encode("UTF-8"), tagfile.section["Maintainer"])
            tagfile.step()
            self.assertEqual(value.encode("ISO-8859-1"), tagfile.section["Maintainer"])


class TestTagSection(testcommon.TestCase):
    """test the apt_pkg.TagFile"""

    def setUp(self):
        testcommon.TestCase.setUp(self)
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def test_write(self):
        ts = apt_pkg.TagSection("a: 1\nb: 2\nc: 3\n")
        outpath = os.path.join(self.temp_dir, "test")
        with open(outpath, "w") as outfile:
            ts.write(outfile, [], [])
        with open(outpath) as outfile:
            self.assertEqual(outfile.read(), "a: 1\nb: 2\nc: 3\n")

    def test_write_order(self):
        ts = apt_pkg.TagSection("a: 1\nb: 2\nc: 3\n")
        outpath = os.path.join(self.temp_dir, "test")
        with open(outpath, "w") as outfile:
            ts.write(outfile, ["a", "c", "b"], [])
        with open(outpath) as outfile:
            self.assertEqual(outfile.read(), "a: 1\nc: 3\nb: 2\n")

    def test_write_invalid_order(self):
        ts = apt_pkg.TagSection("a: 1\nb: 2\nc: 3\n")
        outpath = os.path.join(self.temp_dir, "test")
        with open(outpath, "w") as outfile:
            self.assertRaises(TypeError, ts.write, outfile, ["a", 1, "b"], [])

    def test_write_remove(self):
        ts = apt_pkg.TagSection("a: 1\nb: 2\nc: 3\n")
        outpath = os.path.join(self.temp_dir, "test")
        with open(outpath, "w") as outfile:
            ts.write(outfile, ["a", "c", "b"], [apt_pkg.TagRemove("a")])
        with open(outpath) as outfile:
            self.assertEqual(outfile.read(), "c: 3\nb: 2\n")

    def test_write_rewrite(self):
        ts = apt_pkg.TagSection("a: 1\nb: 2\nc: 3\n")
        outpath = os.path.join(self.temp_dir, "test")
        with open(outpath, "w") as outfile:
            ts.write(outfile, ["a", "c", "b"], [apt_pkg.TagRewrite("a", "AA")])
        with open(outpath) as outfile:
            self.assertEqual(outfile.read(), "a: AA\nc: 3\nb: 2\n")

    def test_write_rename(self):
        ts = apt_pkg.TagSection("a: 1\nb: 2\nc: 3\n")
        outpath = os.path.join(self.temp_dir, "test")
        with open(outpath, "w") as outfile:
            ts.write(outfile, ["a", "z", "b"], [apt_pkg.TagRename("c", "z")])
        with open(outpath) as outfile:
            self.assertEqual(outfile.read(), "a: 1\nz: 3\nb: 2\n")


if __name__ == "__main__":
    unittest.main()
