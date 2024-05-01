#!/usr/bin/python3

import io
import os
import tempfile
import unittest

import apt_pkg
import testcommon

import aptsources.distro
import aptsources.sourceslist


class TestAptSources(testcommon.TestCase):
    def setUp(self):
        testcommon.TestCase.setUp(self)
        self.tempfile = tempfile.NamedTemporaryFile(suffix=".sources")
        apt_pkg.config.set("Dir::Etc::sourcelist", self.tempfile.name)
        apt_pkg.config.set("Dir::Etc::sourceparts", "/dev/null")

    def tearDown(self):
        self.tempfile.close()

    def testEmptyDeb822(self):
        """aptsources: Test sources.list parsing."""
        sources = aptsources.sourceslist.SourcesList(True)
        self.assertListEqual(sources.list, [])

    def testDeb822SectionRecognizedWithoutEndLine(self):
        """aptsources: Test sources.list parsing."""
        section = aptsources._deb822.Section("key: value\notherkey: othervalue")

        # Writing it back out gives us an extra newline at the end
        self.assertEqual(section["key"], "value")
        self.assertEqual(section["otherkey"], "othervalue")
        self.assertEqual(str(section), "key: value\notherkey: othervalue\n")

        file = aptsources._deb822.File(io.StringIO("key: value\notherkey: othervalue"))
        self.assertEqual(len(file.sections), 1)

        section = next(iter(file))
        self.assertEqual(section["key"], "value")
        self.assertEqual(section["otherkey"], "othervalue")
        self.assertEqual(str(section), "key: value\notherkey: othervalue\n")

    def testDeb822MultipleLinesSeparator(self):
        """aptsources: Test sources.list parsing."""
        for separator in "\n\n\n\n", "\n\n\n", "\n\n":
            with self.subTest(f"{len(separator)} separators"):
                file = aptsources._deb822.File(
                    io.StringIO("key: value" + separator + "otherkey: othervalue\n")
                )
                self.assertEqual(len(file.sections), 2)

                self.assertEqual(file.sections[0]["key"], "value")
                self.assertEqual(file.sections[1]["otherkey"], "othervalue")
                self.assertEqual(str(file), "key: value\n\notherkey: othervalue\n")


if __name__ == "__main__":
    os.chdir(os.path.dirname(__file__))
    unittest.main()
