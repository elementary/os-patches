#!/usr/bin/python

import apt

import unittest
import sys
try:
    from urllib.request import urlopen
    from urllib.error import HTTPError, URLError
except ImportError:
    from urllib2 import HTTPError, URLError, urlopen
try:
    from http.client import HTTPException
except ImportError:
    from httplib import HTTPException

sys.path.insert(0, "..")

from softwareproperties.SoftwareProperties import shortcut_handler
from softwareproperties.shortcuts import ShortcutException
from mock import patch

def has_network():
    try:
        network = urlopen("https://launchpad.net/")
        network
    except (URLError, HTTPException):
        return False
    return True

class ShortcutsTestcase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        for k in apt.apt_pkg.config.keys():
            apt.apt_pkg.config.clear(k)
        apt.apt_pkg.init()

    def setUp(self):
        # avoid polution from other tests
        apt.apt_pkg.config.set("Dir", "/")
        apt.apt_pkg.config.set("Dir::Etc", "etc/apt")
        apt.apt_pkg.config.set("Dir::Etc::sourcelist", "sources.list")
        apt.apt_pkg.config.set("Dir::Etc::sourceparts", "sources.list.d")

    def test_shortcut_none(self):
        line = "deb http://ubuntu.com/ubuntu trusty main"
        handler = shortcut_handler(line)
        self.assertEqual((line, None), handler.expand())

    @unittest.skipUnless(has_network(), "requires network")
    def test_shortcut_ppa(self):
        line = "ppa:mvo"
        handler = shortcut_handler(line)
        self.assertEqual(
            ('deb http://ppa.launchpad.net/mvo/ppa/ubuntu trusty main',
             '/etc/apt/sources.list.d/mvo-ubuntu-ppa-trusty.list'),
            handler.expand("trusty", distro="ubuntu"))

    @unittest.skipUnless(has_network(), "requires network")
    def test_shortcut_cloudarchive(self):
        line = "cloud-archive:folsom"
        handler = shortcut_handler(line)
        self.assertEqual(
            ('deb http://ubuntu-cloud.archive.canonical.com/ubuntu '\
             'precise-updates/folsom main',
             '/etc/apt/sources.list.d/cloudarchive-folsom.list'),
            handler.expand("precise", distro="ubuntu"))

    def test_shortcut_exception(self):
        with self.assertRaises(ShortcutException):
            with patch('softwareproperties.ppa.get_ppa_info_from_lp',
                       side_effect=lambda *args: HTTPError("url", 404, "not found", [], None)):
                shortcut_handler("ppa:mvo")



if __name__ == "__main__":
    unittest.main()
