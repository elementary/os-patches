#!/usr/bin/python
import unittest

import apt_pkg
import apt.progress.base

import testcommon


class TestCache(testcommon.TestCase):
    """Test invocation of apt_pkg.Cache()"""

    def test_wrong_invocation(self):
        """cache_invocation: Test wrong invocation."""
        apt_cache = apt_pkg.Cache(progress=None)

        self.assertRaises(ValueError, apt_pkg.Cache, apt_cache)
        self.assertRaises(ValueError, apt_pkg.Cache,
                          apt.progress.base.AcquireProgress())
        self.assertRaises(ValueError, apt_pkg.Cache, 0)

    def test_proper_invocation(self):
        """cache_invocation: Test correct invocation."""
        apt_cache = apt_pkg.Cache(progress=None)
        apt_depcache = apt_pkg.DepCache(apt_cache)
        self.assertNotEqual(apt_depcache, None)

if __name__ == "__main__":
    unittest.main()
