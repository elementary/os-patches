#
# Test that both unicode and bytes path names work
#
import os
import shutil
import unittest
import warnings

import apt_inst
import apt_pkg

import testcommon


class TestPath(testcommon.TestCase):

    dir_unicode = u'data/tmp'
    dir_bytes = b'data/tmp'
    file_unicode = u'data/tmp/python-apt-test'
    file_bytes = b'data/tmp/python-apt-test'

    def setUp(self):
        testcommon.TestCase.setUp(self)
        if os.path.exists(self.dir_bytes):
            shutil.rmtree(self.dir_bytes)

        os.mkdir(self.dir_bytes)

    def tearDown(self):
        apt_pkg.config["dir"] = "/"
        shutil.rmtree(self.dir_bytes)

    def test_acquire(self):
        apt_pkg.AcquireFile(apt_pkg.Acquire(), "http://example.com",
                            destdir=self.file_bytes, destfile=self.file_bytes)
        apt_pkg.AcquireFile(apt_pkg.Acquire(),
                            "http://example.com",
                            destdir=self.file_unicode,
                            destfile=self.file_unicode)

    def test_acquire_file_md5_keyword_backward_compat(self):
        """
        Ensure that both "md5" and "hash" is supported as keyword for
        AcquireFile
        """
        with warnings.catch_warnings(record=True) as caught_warnings:
            warnings.simplefilter("always")
            apt_pkg.AcquireFile(
                apt_pkg.Acquire(), "http://example.com",
                destfile=self.file_bytes,
                md5="abcdef")

        self.assertEqual(len(caught_warnings), 1)
        self.assertTrue(issubclass(caught_warnings[0].category,
                                   DeprecationWarning))
        self.assertIn("md5", str(caught_warnings[0].message))
        self.assertIn("hash", str(caught_warnings[0].message))

        apt_pkg.AcquireFile(
            apt_pkg.Acquire(), "http://example.com",
            destfile=self.file_bytes,
            hash="sha1:41050ed528554fdd6c6c9a086ddd6bdba5857b21")

    def test_ararchive(self):
        archive = apt_inst.ArArchive(u"data/test_debs/data-tar-xz.deb")

        apt_inst.ArArchive(b"data/test_debs/data-tar-xz.deb")

        archive.extract(u"debian-binary", u"data/tmp")
        archive.extract(b"debian-binary", b"data/tmp")
        archive.extractall(u"data/tmp")
        archive.extractall(b"data/tmp")
        self.assertEqual(archive.extractdata(u"debian-binary"), b"2.0\n")
        self.assertEqual(archive.extractdata(b"debian-binary"), b"2.0\n")
        self.assertTrue(archive.getmember(u"debian-binary"))
        self.assertTrue(archive.getmember(b"debian-binary"))
        self.assertTrue(u"debian-binary" in archive)
        self.assertTrue(b"debian-binary" in archive)
        self.assertTrue(archive[b"debian-binary"])
        self.assertTrue(archive[u"debian-binary"])

        tar = archive.gettar(u"control.tar.xz", "xz")
        tar = archive.gettar(b"control.tar.xz", "xz")

        tar.extractall(self.dir_unicode)
        tar.extractall(self.dir_bytes)
        self.assertRaises(LookupError, tar.extractdata, u"Do-not-exist")
        self.assertRaises(LookupError, tar.extractdata, b"Do-not-exist")
        tar.extractdata(b"control")
        tar.extractdata(u"control")

        apt_inst.TarFile(os.path.join(self.dir_unicode, u"control.tar.xz"))
        apt_inst.TarFile(os.path.join(self.dir_bytes, b"control.tar.xz"))

    def test_configuration(self):
        with open(self.file_unicode, 'w') as config:
            config.write("Hello { World 1; };")
        apt_pkg.read_config_file(apt_pkg.config, self.file_bytes)
        apt_pkg.read_config_file(apt_pkg.config, self.file_unicode)
        apt_pkg.read_config_file_isc(apt_pkg.config, self.file_bytes)
        apt_pkg.read_config_file_isc(apt_pkg.config, self.file_unicode)
        apt_pkg.read_config_dir(apt_pkg.config, self.dir_unicode)
        apt_pkg.read_config_dir(apt_pkg.config, b"/etc/apt/apt.conf.d")

    def test_index_file(self):
        apt_pkg.config["dir"] = "data/test_debs"
        slist = apt_pkg.SourceList()
        slist.read_main_list()

        for meta in slist.list:
            for index in meta.index_files:
                index.archive_uri(self.file_bytes)
                index.archive_uri(self.file_unicode)

    def test_lock(self):
        apt_pkg.get_lock(self.file_unicode, True)
        apt_pkg.get_lock(self.file_bytes, True)

        with apt_pkg.FileLock(self.file_unicode):
            pass
        with apt_pkg.FileLock(self.file_bytes):
            pass

    def test_policy(self):
        apt_pkg.config["dir"] = "data/test_debs"
        cache = apt_pkg.Cache(None)
        policy = apt_pkg.Policy(cache)
        file_unicode = os.path.join(self.dir_unicode, u"test.prefs")
        file_bytes = os.path.join(self.dir_bytes, b"test.prefs")

        self.assertTrue(policy.read_pinfile(file_unicode))
        self.assertTrue(policy.read_pinfile(file_bytes))
        self.assertTrue(policy.read_pindir(self.dir_unicode))
        self.assertTrue(policy.read_pindir(self.dir_bytes))

    def test_tag(self):
        with open(self.file_bytes, "w") as tag:
            tag.write("Key: value\n")
        tag1 = apt_pkg.TagFile(self.file_unicode)
        tag2 = apt_pkg.TagFile(self.file_bytes)

        self.assertEqual(next(tag1)["Key"], "value")
        self.assertEqual(next(tag2)["Key"], "value")

        self.assertRaises(StopIteration, next, tag1)
        self.assertRaises(StopIteration, next, tag2)

if __name__ == '__main__':
    unittest.main()
