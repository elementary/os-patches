#!/usr/bin/python3
#
# Copyright (C) 2018 Michael Vogt <mvo@ubuntu.com>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of source records in apt_pkg."""

import os
import shutil
import sys
import unittest

from test_all import get_library_dir

libdir = get_library_dir()
if libdir:
    sys.path.insert(0, libdir)

import apt_pkg
import testcommon

import apt


class TestSourceRecords(testcommon.TestCase):
    def setUp(self):
        testcommon.TestCase.setUp(self)

        rootdir = "./data/tmp"
        if os.path.exists(rootdir):
            shutil.rmtree(rootdir)
        try:
            os.makedirs(os.path.join(rootdir, "etc", "apt"))
        except OSError:
            pass

        for k in apt_pkg.config.keys():
            apt_pkg.config.clear(k)

        apt_pkg.config["Dir"] = os.path.abspath(rootdir)
        apt_pkg.init_config()

        # set a local sources.list that does not need the network
        base_sources = os.path.abspath(
            os.path.join(rootdir, "etc", "apt", "sources.list")
        )
        # main sources.list
        sources_list = base_sources
        with open(sources_list, "w") as f:
            repo = os.path.abspath("./data/test-source-repo")
            f.write("deb-src [trusted=yes] copy:%s /\n" % repo)

        self.assertTrue(os.path.exists(sources_list))

        # update a single sources.list
        cache = apt.Cache(rootdir=rootdir)
        cache.update(sources_list=sources_list)

    def test_source_records_smoke(self):
        src = apt_pkg.SourceRecords()
        self.assertTrue(src.step())

        self.assertEqual(src.maintainer, "Julian Andres Klode <jak@debian.org>")  # noqa
        self.assertEqual(src.binaries, ["dh-autoreconf"])
        self.assertEqual(src.package, "dh-autoreconf")

        self.assertEqual(2, len(src.files))

        # unpacking as a tuple works as before
        md5, size, path, type_ = f = src.files[0]
        self.assertEqual(md5, None)
        self.assertEqual(size, 1578)
        self.assertEqual(path, "dh-autoreconf_16.dsc")
        self.assertEqual(type_, "dsc")
        # access using getters
        self.assertTrue(isinstance(f.hashes, apt_pkg.HashStringList))
        self.assertEqual(
            str(f.hashes[0]),
            "SHA512:4b1a3299f2a8b01b0c75db97fd16cb39919949c74d19ea6cf28e1bbd4891d3515b3e2b90b96a64df665cebf6d95409e704e670909ae91fcfe92409ee1339bffc",
        )  # noqa
        self.assertEqual(str(f.hashes[1]), "Checksum-FileSize:1578")
        self.assertEqual(
            str(f.hashes[2]),
            "SHA256:1c1b2ab5f1ae5496bd50dbb3c30e9b7d181a06c8d02ee8d7e9c35ed6f2a69b5f",
        )  # noqa
        self.assertEqual(
            str(f.hashes[3]), "SHA1:c9bf7a920013021dad5fbd898dfd5a79c7a150f9"
        )  # noqa
        self.assertEqual(
            str(f.hashes[4]), "MD5Sum:6576a28fe1918ce10bd31543ba545901"
        )  # noqa
        self.assertEqual(f.size, 1578)
        self.assertEqual(f.path, "dh-autoreconf_16.dsc")
        self.assertEqual(f.type, "dsc")

        # unpacking as a tuple works as before
        md5, size, path, type_ = f = src.files[1]
        self.assertEqual(md5, None)
        self.assertEqual(size, 7372)
        self.assertEqual(path, "dh-autoreconf_16.tar.xz")
        self.assertEqual(type_, "tar")
        # access using getters
        self.assertTrue(isinstance(f.hashes, apt_pkg.HashStringList))
        self.assertEqual(
            str(f.hashes[0]),
            "SHA512:10448dd179ec12bf4310a9a514110a85f56e51893aa36a97ac3a6f8d7ce99d099e62cfdb78e271e2d94431e8832da0f643de821b6643b80e3f0b0f5d682cf9a9",
        )  # noqa
        self.assertEqual(str(f.hashes[1]), "Checksum-FileSize:7372")  # noqa
        self.assertEqual(
            str(f.hashes[2]),
            "SHA256:5c6a6a362907327bec77a867ff3fd0eceba8015d1b881b48275aff7e4ce0f629",
        )  # noqa
        self.assertEqual(
            str(f.hashes[3]), "SHA1:58459600164398ad6807ddd877a6f814c799c62c"
        )  # noqa
        self.assertEqual(
            str(f.hashes[4]), "MD5Sum:302c8bf43db02412e3f2197fd0f2ee0f"
        )  # noqa
        self.assertEqual(f.size, 7372)
        self.assertEqual(f.path, "dh-autoreconf_16.tar.xz")
        self.assertEqual(f.type, "tar")

        self.assertFalse(src.step())


if __name__ == "__main__":
    unittest.main()
