#!/usr/bin/python

import unittest
import apt_pkg


class testHashes(unittest.TestCase):
    " test the hashsum functions against strings and files "

    DATA_PATH = "data/hashsums/hashsum_test.data"
    DATA_WITH_ZERO_PATH = "data/hashsums/hashsum_test_with_zero.data"

    def testMD5(self):
        # simple
        s = b"foo"
        s_md5 = "acbd18db4cc2f85cedef654fccc4a4d8"
        res = apt_pkg.md5sum(s)
        self.assertEqual(res, s_md5)
        # file
        with open(self.DATA_PATH) as fobj:
            self.assertEqual(apt_pkg.md5sum(fobj), s_md5)
        # with zero (\0) in the string
        s = b"foo\0bar"
        s_md5 = "f6f5f8cd0cb63668898ba29025ae824e"
        res = apt_pkg.md5sum(s)
        self.assertEqual(res, s_md5)
        # file
        with open(self.DATA_WITH_ZERO_PATH) as fobj:
            self.assertEqual(apt_pkg.md5sum(fobj), s_md5)

    def testSHA1(self):
        # simple
        s = b"foo"
        s_hash = "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33"
        res = apt_pkg.sha1sum(s)
        self.assertEqual(res, s_hash)
        # file
        with open(self.DATA_PATH) as fobj:
            self.assertEqual(apt_pkg.sha1sum(fobj), s_hash)
        # with zero (\0) in the string
        s = b"foo\0bar"
        s_hash = "e2c300a39311a2dfcaff799528415cb74c19317f"
        res = apt_pkg.sha1sum(s)
        self.assertEqual(res, s_hash)
        # file
        with open(self.DATA_WITH_ZERO_PATH) as fobj:
            self.assertEqual(apt_pkg.sha1sum(fobj), s_hash)

    def testSHA256(self):
        # simple
        s = b"foo"
        s_hash = \
            "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae"
        res = apt_pkg.sha256sum(s)
        self.assertEqual(res, s_hash)
        # file
        with open(self.DATA_PATH) as fobj:
            self.assertEqual(apt_pkg.sha256sum(fobj), s_hash)
        # with zero (\0) in the string
        s = b"foo\0bar"
        s_hash = \
            "d6b681bfce7155d44721afb79c296ef4f0fa80a9dd6b43c5cf74dd0f64c85512"
        res = apt_pkg.sha256sum(s)
        self.assertEqual(res, s_hash)
        # file
        with open(self.DATA_WITH_ZERO_PATH) as fobj:
            self.assertEqual(apt_pkg.sha256sum(fobj), s_hash)

if __name__ == "__main__":
    unittest.main()
