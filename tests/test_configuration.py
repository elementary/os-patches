#!/usr/bin/python3
#
# Copyright (C) 2011 Julian Andres Klode <jak@debian.org>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of apt_pkg.Configuration"""
import unittest
import apt_pkg

import testcommon


class TestConfiguration(testcommon.TestCase):
    """Test various configuration things"""

    def test_lp707416(self):
        """configuration: Test empty arguments (LP: #707416)"""
        self.assertRaises(ValueError, apt_pkg.parse_commandline,
                          apt_pkg.config, [], [])
        self.assertRaises(SystemError, apt_pkg.parse_commandline,
                          apt_pkg.config, [], ["cmd", "--arg0"])


if __name__ == "__main__":
    unittest.main()
