#!/usr/bin/python3
import unittest

import apt_pkg
import testcommon


class TestLargeFileSupport(testcommon.TestCase):
    """Test large file support"""

    def test_acquire_file(self):
        """Test apt_pkg.AcquireFile() accepts large file size"""
        acq = apt_pkg.Acquire()
        fil = apt_pkg.AcquireFile(acq, "http://foo", "foo", size=2875204834)
        self.assertEqual(fil.filesize, 2875204834)


if __name__ == "__main__":
    unittest.main()
