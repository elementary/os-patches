#!/usr/bin/python3
#
# Copyright (C) 2009 Julian Andres Klode <jak@debian.org>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of hashsums in apt_pkg.

Unit tests to verify the correctness of Hashes, HashString and the various
functions like md5sum."""
import hashlib
import unittest
import warnings

import apt_pkg
import testcommon


class TestHashes(testcommon.TestCase):
    """Test apt_pkg.Hashes() and the various apt_pkg.*sum() functions."""

    def setUp(self):
        """Prepare the tests, create reference values..."""
        testcommon.TestCase.setUp(self)
        self.file = open(apt_pkg.__file__, "rb")
        self.value = self.file.read()
        self.hashes = apt_pkg.Hashes(self.value)
        self.file.seek(0)
        self.fhashes = apt_pkg.Hashes(self.file)
        # Reference values.
        self.md5 = hashlib.md5(self.value).hexdigest()
        self.sha1 = hashlib.sha1(self.value).hexdigest()
        self.sha256 = hashlib.sha256(self.value).hexdigest()
        self.file.seek(0)

        warnings.filterwarnings("ignore", category=DeprecationWarning)

    def tearDown(self):
        """Cleanup, Close the file object used for the tests."""
        testcommon.TestCase.tearDown(self)
        warnings.resetwarnings()
        self.file.close()

    def test_md5sum(self):
        """hashes: Test apt_pkg.md5sum()"""
        self.assertEqual(apt_pkg.md5sum(self.value), self.md5)
        self.assertEqual(apt_pkg.md5sum(self.file), self.md5)

    def test_sha1sum(self):
        """hashes: Test apt_pkg.sha1sum()"""
        self.assertEqual(apt_pkg.sha1sum(self.value), self.sha1)
        self.assertEqual(apt_pkg.sha1sum(self.file), self.sha1)

    def test_sha256sum(self):
        """hashes: Test apt_pkg.sha256sum()"""
        self.assertEqual(apt_pkg.sha256sum(self.value), self.sha256)
        self.assertEqual(apt_pkg.sha256sum(self.file), self.sha256)

    def test_bytes(self):
        """hashes: Test apt_pkg.Hashes(bytes)"""
        self.assertEqual(self.hashes.hashes.find("md5sum").hashvalue, self.md5)
        self.assertEqual(
            self.hashes.hashes.find("md5sum"), apt_pkg.HashString("MD5Sum", self.md5)
        )
        self.assertEqual(
            self.hashes.hashes.find("sha1"), apt_pkg.HashString("SHA1", self.sha1)
        )
        self.assertEqual(
            self.hashes.hashes.find("sha256"), apt_pkg.HashString("SHA256", self.sha256)
        )
        self.assertRaises(KeyError, self.hashes.hashes.find, "md5")

    def test_file(self):
        """hashes: Test apt_pkg.Hashes(file)."""
        self.assertEqual(self.fhashes.hashes.find("md5sum").hashvalue, self.md5)
        self.assertEqual(
            self.fhashes.hashes.find("md5sum"), apt_pkg.HashString("MD5Sum", self.md5)
        )
        self.assertEqual(
            self.fhashes.hashes.find("sha1"), apt_pkg.HashString("SHA1", self.sha1)
        )
        self.assertEqual(
            self.fhashes.hashes.find("sha256"),
            apt_pkg.HashString("SHA256", self.sha256),
        )

    def test_unicode(self):
        """hashes: Test apt_pkg.Hashes(unicode)."""
        self.assertRaises(TypeError, apt_pkg.Hashes, "D")
        self.assertRaises(TypeError, apt_pkg.md5sum, "D")
        self.assertRaises(TypeError, apt_pkg.sha1sum, "D")
        self.assertRaises(TypeError, apt_pkg.sha256sum, "D")


class TestHashString(testcommon.TestCase):
    """Test apt_pkg.HashString()."""

    def setUp(self):
        """Prepare the test by reading the file."""
        testcommon.TestCase.setUp(self)
        self.file = open(apt_pkg.__file__)
        self.hashes = apt_pkg.Hashes(self.file)

        self.md5 = self.hashes.hashes.find("md5sum")
        self.sha1 = self.hashes.hashes.find("sha1")
        self.sha256 = self.hashes.hashes.find("sha256")

    def tearDown(self):
        """Cleanup, Close the file object used for the tests."""
        self.file.close()

    def test_md5(self):
        """hashes: Test apt_pkg.HashString().md5"""
        self.assertIn("MD5Sum:", str(self.md5))
        self.assertTrue(self.md5.verify_file(apt_pkg.__file__))

    def test_sha1(self):
        """hashes: Test apt_pkg.HashString().sha1"""
        self.assertIn("SHA1:", str(self.sha1))
        self.assertTrue(self.sha1.verify_file(apt_pkg.__file__))

    def test_sha256(self):
        """hashes: Test apt_pkg.HashString().sha256"""
        self.assertIn("SHA256:", str(self.sha256))
        self.assertTrue(self.sha256.verify_file(apt_pkg.__file__))

    def test_wrong(self):
        """hashes: Test apt_pkg.HashString(wrong_type)."""
        self.assertRaises(TypeError, apt_pkg.HashString, 0)
        self.assertRaises(TypeError, apt_pkg.HashString, b"")


class TestHashStringList(testcommon.TestCase):
    """Test apt_pkg.HashStringList()"""

    def test_file_size(self):
        hsl = apt_pkg.HashStringList()
        self.assertEqual(hsl.file_size, 0)
        hsl.file_size = 42
        self.assertEqual(hsl.file_size, 42)
        self.assertEqual(len(hsl), 1)

        # Verify that I can re-assign value (this handles the long case on
        # Python 2).
        hsl.file_size = hsl.file_size

        with self.assertRaises(OverflowError):
            hsl.file_size = -1

        hsl.file_size = 0

    def test_append(self):
        """Testing whether append works correctly."""
        hs1 = apt_pkg.HashString("MD5Sum", "a60599e6200b60050d7a30721e3532ed")
        hs2 = apt_pkg.HashString("SHA1", "ef113338e654b1ada807a939ad47b3a67633391b")

        hsl = apt_pkg.HashStringList()
        hsl.append(hs1)
        hsl.append(hs2)
        self.assertEqual(len(hsl), 2)
        self.assertEqual(hsl[0].hashtype, "MD5Sum")
        self.assertEqual(hsl[1].hashtype, "SHA1")
        self.assertEqual(str(hsl[0]), str(hs1))
        self.assertEqual(str(hsl[1]), str(hs2))

    def test_find(self):
        """Testing whether append works correctly."""
        hs1 = apt_pkg.HashString("MD5Sum", "a60599e6200b60050d7a30721e3532ed")
        hs2 = apt_pkg.HashString("SHA1", "ef113338e654b1ada807a939ad47b3a67633391b")

        hsl = apt_pkg.HashStringList()
        hsl.append(hs1)
        hsl.append(hs2)

        self.assertEqual(hsl.find("MD5Sum").hashtype, "MD5Sum")
        self.assertEqual(hsl.find("SHA1").hashtype, "SHA1")
        self.assertEqual(hsl.find().hashtype, "SHA1")

    def test_verify_file(self):
        with open(apt_pkg.__file__) as fobj:
            hashes = apt_pkg.Hashes(fobj)
            with warnings.catch_warnings(record=True):
                warnings.simplefilter("always")
                sha1 = hashes.hashes.find("sha1")
                sha256 = hashes.hashes.find("sha256")

        hsl = apt_pkg.HashStringList()
        hsl.append(sha1)
        hsl.append(sha256)

        self.assertTrue(hsl.verify_file(apt_pkg.__file__))

        md5sum = apt_pkg.HashString("MD5Sum", "a60599e6200b60050d7a30721e3532ed")
        hsl.append(md5sum)

        self.assertFalse(hsl.verify_file(apt_pkg.__file__))

        hsl2 = hashes.hashes
        self.assertIsInstance(hsl2, apt_pkg.HashStringList)
        self.assertGreater(len(hsl2), 0)
        self.assertTrue(hsl2.verify_file(apt_pkg.__file__))


if __name__ == "__main__":
    unittest.main()
