#!/usr/bin/python
# -*- coding: utf-8 -*-

import apt_pkg

import logging
from mock import patch
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, "../")
from softwareproperties.SoftwareProperties import (
    SoftwareProperties)

class SoftwarePropertiesTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        for k in apt_pkg.config.keys():
            apt_pkg.config.clear(k)
        apt_pkg.init()

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.sp = SoftwareProperties()
        self.mock_key = os.path.join(self.temp_dir, u"määäp.asc")
        with open(self.mock_key, "wb") as fp:
            fp.write(u"bäää".encode("utf-8"))

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def test_add_key_str(self):
        with patch.object(self.sp, "apt_key") as mock_apt_key:
            self.sp.add_key(self.mock_key)
        self.assertTrue(mock_apt_key.add.called)

    def test_add_key_bytes(self):
        with patch.object(self.sp, "apt_key") as mock_apt_key:
            mock_name_as_bytes = self.mock_key.encode("utf-8")
            self.sp.add_key(mock_name_as_bytes)
        self.assertTrue(mock_apt_key.add.called)


if __name__ == "__main__":
    if "-d" in sys.argv:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
    unittest.main()
