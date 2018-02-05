#!/usr/bin/python

import sys
import unittest

import apt_inst

import testcommon

IS_NOT_32BIT = sys.maxsize > 2 ** 32


@unittest.skipIf(IS_NOT_32BIT, "Large File support is for 32 bit systems")
class testHashes(testcommon.TestCase):
    " test the hashsum functions against strings and files "

    LARGE_PACKAGE_CONTENT = "data/test_debs/large-package-content_1.0_all.deb"

    def testExtractData(self):
        deb = apt_inst.DebFile(self.LARGE_PACKAGE_CONTENT)

        self.assertRaises(MemoryError, deb.data.extractdata, "large-file")

if __name__ == "__main__":
    unittest.main()
