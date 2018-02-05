#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2010 Michael Vogt <mvo@ubuntu.com>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of DebPackage in apt.debfile."""
import os
import logging
import unittest

from test_all import get_library_dir
import sys
libdir = get_library_dir()
if libdir:
    sys.path.insert(0, libdir)
import apt_pkg
import apt.debfile

import testcommon


class TestDebfile(testcommon.TestCase):
    """ test the debfile """

    TEST_DEBS = [
        # conflicts with apt
        ('gdebi-test1.deb', False),
        # impossible dependency
        ('gdebi-test2.deb', False),
        #  or-group (impossible-dependency|apt)
        ('gdebi-test3.deb', True),
        # Conflicts: apt (<= 0.1)
        ('gdebi-test4.deb', True),
        # Conflicts: apt (>= 0.1)
        ('gdebi-test5.deb', False),
        # invalid unicode  in descr
        ('gdebi-test6.deb', True),
        # provides/conflicts against "foobarbaz"
        ('gdebi-test7.deb', True),
        # provides/conflicts/replaces against "mail-transport-agent"
        # (should fails if mail-transport-agent is installed)
        ('gdebi-test8.deb', False),
        # provides/conflicts against real pkg
        ('gdebi-test9.deb', True),
        # provides debconf-tiny and the real debconf conflicts with
        ('gdebi-test10.deb', False),
    ]

    def setUp(self):
        testcommon.TestCase.setUp(self)
        apt_pkg.config.set("APT::Architecture", "i386")
        # FIXME: When run via test_all.py, the tests fail without this if it
        # is set in the system.
        apt_pkg.config.clear("APT::Architectures")
        apt_pkg.config.set("Dir::State::status",
                           "./data/test_debs/var/lib/dpkg/status")
        apt_pkg.config.set("Dir::State::lists",
                           "./data/test_debs/var/lib/apt/lists")
        apt_pkg.config.set("Dir::Etc::sourcelist",
                           "./data/test_debs/etc/apt/sources.list")
        apt_pkg.init_system()
        self.cache = apt.Cache()

    def test_dsc_file(self):
        filename = "hello_2.5-1.dsc"
        deb = apt.debfile.DscSrcPackage(cache=self.cache)
        deb.open(os.path.join("data", "test_debs", filename))
        self.assertTrue(deb.check(), "got failure '%s'" % deb._failure_string)
        missing = set(['autotools-dev'])
        self.assertEqual(set(deb.missing_deps), missing)
        # specialized properties
        self.assertEqual(deb.pkgname, "hello")
        self.assertEqual(deb.binaries, ["hello", "bello", "cello"])
        self.assertEqual(deb.filelist, ["hello_2.5.orig.tar.gz",
                                        "hello_2.5-1.diff.gz"])
        self.assertEqual(deb.depends, [[("autotools-dev", "", "")]])
        # tag fields are available as a dict
        self.assertEqual(deb["Format"], "1.0")
        self.assertEqual(deb["Source"], "hello")
        self.assertEqual(deb["Binary"], "hello, bello,\n cello")
        self.assertEqual(deb["Architecture"], "any")
        self.assertEqual(deb["Version"], "2.5-1")
        self.assertEqual(
            deb["Maintainer"], "Santiago Vila <sanvila@debian.org>")
        self.assertEqual(deb["Homepage"], "http://www.gnu.org/software/hello")
        self.assertEqual(deb["Standards-Version"], "3.8.4")

    def test_dsc_file_with_impossible_build_dep(self):
        filename = "impossible-build-depends_2.5-1.dsc"
        deb = apt.debfile.DscSrcPackage(cache=self.cache)
        deb.open(os.path.join("data", "test_debs", filename))
        self.assertFalse(deb.check())
        self.assertEqual(deb.depends, [[("debhelper", "101", ">")]])

    def test_deb_file(self):
        deb = apt.debfile.DebPackage(cache=self.cache)
        for (filename, expected_res) in self.TEST_DEBS:
            logging.debug("testing %s, expecting %s" % (
                filename, expected_res))
            deb.open(os.path.join("data", "test_debs", filename))
            res = deb.check()
            self.assertEqual(res, expected_res,
                "Unexpected result for package '%s' (got %s wanted %s)\n%s" % (
                    filename, res, expected_res, deb._failure_string))

    def test_utf8_sections(self):
        deb = apt.debfile.DebPackage(cache=self.cache)
        deb.open(
            os.path.join("data", "test_debs", "utf8-package_1.0-1_all.deb"))
        self.assertEqual(deb["Maintainer"],
                         "Samuel Lidén Borell <samuel@slbdata.se>")

    def test_content(self):
        # normal
        deb = apt.debfile.DebPackage(cache=self.cache)
        deb.open(os.path.join("data", "test_debs", "gdebi-test11.deb"))
        self.assertEqual('#!/bin/sh\necho "test"\n',
                         deb.data_content("usr/bin/test"))
        # binary
        deb = apt.debfile.DebPackage(cache=self.cache)
        deb.open(os.path.join("data", "test_debs", "gdebi-test12.deb"))
        content = deb.data_content("usr/bin/binary")
        self.assertTrue(content.startswith(
            "Automatically converted to printable ascii:\n\x7fELF "))
        # control file
        needle = """Package: gdebi-test12
Version: 1.0
Architecture: all
Description: testpackage for gdebi - contains usr/bin/binary for file reading
 This tests the binary file reading for debfile.py
"""
        content = deb.control_content("./control")
        self.assertEqual(content, needle)
        content = deb.control_content("control")
        self.assertEqual(content, needle)

    def test_xz_data(self):
        deb = apt.debfile.DebPackage("./data/test_debs/data-tar-xz.deb")
        self.assertEqual(deb.filelist, ["./", "usr/", "usr/bin/"])

    @unittest.skipIf(apt_pkg.version_compare(apt_pkg.VERSION, "0.9.15.4~") < 0,
                     "APT too old for uncompressed control.tar/data.tar")
    def test_uncompressed_data(self):
        deb = apt.debfile.DebPackage("./data/test_debs/data-tar.deb")
        self.assertEqual(deb.filelist, ["./", "usr/", "usr/bin/"])

    def test_check_exception(self):
        deb = apt.debfile.DebPackage("./data/test_debs/data-tar-xz.deb")
        self.assertRaises(AttributeError, lambda: deb.missing_deps)
        deb.check()
        deb.missing_deps

    def test_no_supported_data_tar(self):
        with self.assertRaises(SystemError):
            apt.debfile.DebPackage("./data/test_debs/data-tar-broken.deb")

    def test_contains(self):
        deb = apt.debfile.DebPackage("./data/test_debs/data-tar-xz.deb")
        self.assertTrue("Package" in deb)

    def test_multi_arch_allowed(self):
        apt_pkg.config["APT::Architectures::"] = "i386"
        apt_pkg.config["APT::Architectures::"] = "amd64"
        apt_pkg.config["APT::Architecture"] = "amd64"
        apt_pkg.init_system()

        allowed_any = apt.debfile.DebPackage(
            "./data/test_debs/testdep-allowed-any_1.0-1_i386.deb")
        self.assertTrue(allowed_any.check(), allowed_any._failure_string)

    def test_multi_arch_same(self):
        apt_pkg.config["APT::Architectures::"] = "i386"
        apt_pkg.config["APT::Architectures::"] = "amd64"
        apt_pkg.config["APT::Architecture"] = "amd64"
        apt_pkg.init_system()
        same = apt.debfile.DebPackage(
            "./data/test_debs/testdep-same-arch_1.0-1_i386.deb")
        self.assertTrue(same.check(), same._failure_string)

    def test_get_content_gzip_data(self):
        deb = apt.debfile.DebPackage("./data/test_debs/gdebi-test13.deb")
        data = deb.data_content("./lala.gz")
        self.assertEqual(data, "Automatically decompressed:\n\nlala\n")


if __name__ == "__main__":
    #logging.basicConfig(level=logging.DEBUG)
    unittest.main()
