#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2020 Canonical Ltd
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of DebFile descriptor handling."""
import os
import unittest

from test_all import get_library_dir
import sys

libdir = get_library_dir()
if libdir:
    sys.path.insert(0, libdir)
import apt_inst
import subprocess
import tempfile


@unittest.skipIf(
    not os.path.exists("/proc/self/fd"), "no /proc/self/fd available"
)
class TestCVE_2020_27351(unittest.TestCase):
    """ test the debfile """

    GOOD_DEB = "data/test_debs/utf8-package_1.0-1_all.deb"

    def test_success(self):
        """opening package successfully should not leak fd"""
        before = os.listdir("/proc/self/fd")
        apt_inst.DebFile(self.GOOD_DEB)
        after = os.listdir("/proc/self/fd")
        self.assertEqual(before, after)

    def test_success_a_member(self):
        """fd should be kept around as long as a tarfile member"""
        before = os.listdir("/proc/self/fd")
        data = apt_inst.DebFile(self.GOOD_DEB).data
        after = os.listdir("/proc/self/fd")
        self.assertEqual(len(before), len(after) - 1)
        del data
        after = os.listdir("/proc/self/fd")
        self.assertEqual(before, after)

    def _create_deb_without(self, member):
        temp = tempfile.NamedTemporaryFile(mode="wb")
        try:
            with open(self.GOOD_DEB, "rb") as deb:
                temp.write(deb.read())
            temp.flush()
            subprocess.check_call(["ar", "d", temp.name, member])
            return temp
        except Exception as e:
            temp.close()
            raise e

    def test_nocontrol(self):
        """opening package without control.tar.gz should not leak fd"""
        before = os.listdir("/proc/self/fd")
        with self._create_deb_without("control.tar.gz") as temp:
            try:
                apt_inst.DebFile(temp.name)
            except SystemError as e:
                self.assertIn("control.tar", str(e))
            else:
                self.fail("Did not raise an exception")

        after = os.listdir("/proc/self/fd")
        self.assertEqual(before, after)

    def test_nodata(self):
        """opening package without data.tar.gz should not leak fd"""
        before = os.listdir("/proc/self/fd")
        with self._create_deb_without("data.tar.gz") as temp:
            try:
                apt_inst.DebFile(temp.name)
            except SystemError as e:
                self.assertIn("data.tar", str(e))
            else:
                self.fail("Did not raise an exception")

        after = os.listdir("/proc/self/fd")
        self.assertEqual(before, after)

    def test_no_debian_binary(self):
        """opening package without debian-binary should not leak fd"""
        before = os.listdir("/proc/self/fd")
        with self._create_deb_without("debian-binary") as temp:
            try:
                apt_inst.DebFile(temp.name)
            except SystemError as e:
                self.assertIn("missing debian-binary", str(e))
            else:
                self.fail("Did not raise an exception")

        after = os.listdir("/proc/self/fd")
        self.assertEqual(before, after)


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)
    unittest.main()
