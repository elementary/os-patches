#!/usr/bin/python3
# -*- coding: utf-8 -*-
#
# Copyright (C) 2010 Michael Vogt <mvo@ubuntu.com>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of DebPackage in apt.debfile."""
import unittest

from test_all import get_library_dir
import sys
libdir = get_library_dir()
if libdir:
    sys.path.insert(0, libdir)
import apt
import apt_pkg
import apt.debfile

import testcommon


class TestDebfileMultiarch(testcommon.TestCase):
    """ test the multiarch debfile """

    def test_multiarch_deb_check(self):
        if apt_pkg.get_architectures() != ["amd64", "i386"]:
            # TODO: use unittest.skip
            #logging.warning("skipping test because running on a "
            #                "non-multiarch system")
            return
        deb = apt.debfile.DebPackage(
            "./data/test_debs/multiarch-test1_i386.deb")
        deb.check()
        missing = deb.missing_deps
        #print missing
        self.assertFalse("dpkg:i386" in missing)

    @unittest.skip("BROKEN, lib3ds-1-3 is m-a now")
    def test_multiarch_conflicts(self):
        cache = apt.Cache()
        # WARNING: this assumes that lib3ds-1-3 is a non-multiarch lib
        # use "lib3ds-1-3" as a test to see if non-multiach lib conflicts work
        canary = "lib3ds-1-3"
        if canary not in cache:
            # TODO: use unittest.skip
            #logging.warning("skipping test because %s is missing" % canary)
            return
        cache[canary].mark_install()
        deb = apt.debfile.DebPackage(
            "./data/test_debs/multiarch-test1_i386.deb", cache=cache)
        # this deb should now not be installable
        installable = deb.check()
        #print deb._failure_string
        self.assertFalse(installable)
        self.assertEqual(deb._failure_string,
                         "Conflicts with the installed package 'lib3ds-1-3'")


if __name__ == "__main__":
    unittest.main()
