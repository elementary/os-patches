#!/usr/bin/python3
import os
import tempfile
import unittest

import apt_pkg
import testcommon

import aptsources.distro
import aptsources.sourceslist


class TestAptSourcesPorts(testcommon.TestCase):
    """Test aptsources on ports.ubuntu.com."""

    def setUp(self):
        testcommon.TestCase.setUp(self)
        apt_pkg.config.set("APT::Architecture", "powerpc")
        apt_pkg.config.set("Dir::Etc", os.path.abspath("data/aptsources_ports"))
        apt_pkg.config.set("Dir::Etc::sourceparts", tempfile.mkdtemp())
        if os.path.exists("../build/data/templates"):
            self.templates = os.path.abspath("../build/data/templates")
        else:
            self.templates = "/usr/share/python-apt/templates/"

    def testMatcher(self):
        """aptsources_ports: Test matcher."""
        apt_pkg.config.set("Dir::Etc::sourcelist", "sources.list")
        sources = aptsources.sourceslist.SourcesList(True, self.templates)
        distro = aptsources.distro.get_distro("Ubuntu", "hardy", "desc", "8.04")
        distro.get_sources(sources)
        # test if all suits of the current distro were detected correctly
        for s in sources:
            if not s.line.strip() or s.line.startswith("#"):
                continue
            if not s.template:
                self.fail("source entry '%s' has no matcher" % s)


if __name__ == "__main__":
    unittest.main()
