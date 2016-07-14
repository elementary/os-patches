#!/usr/bin/python3
import apt
import apt_pkg
import logging
import unittest

import sys
sys.path.insert(0, "../")

from LanguageSelector.LocaleInfo import LocaleInfo
from LanguageSelector.LangCache import LanguageSelectorPkgCache, LanguageInformation

class TestLangCache(unittest.TestCase):

    def setUp(self):
        apt_pkg.config.set("Dir::State::lists","./test-data/var/lib/apt/lists.cl")
        apt_pkg.config.set("Dir::State::status","./test-data/var/lib/dpkg/status")
        apt_pkg.config.set("Dir::Etc::SourceList","./test-data/etc/apt/sources.list.cl")
        apt_pkg.config.set("Dir::Etc::SourceParts","x")
        logging.info("updating the cache")
        localeinfo = LocaleInfo("languagelist", "..")
        self.lang_cache = LanguageSelectorPkgCache(
            localeinfo, apt.progress.base.OpProgress())
        self.lang_cache.update()
        self.lang_cache.open()

    def test_get_language_information(self):
        """ test if getLanguagenformation returns values """
        language_info = self.lang_cache.getLanguageInformation()
        self.assertTrue(len(language_info) > 1)
        self.assertEqual(len([x for x in language_info if x.languageCode == "de"]), 1)

    def test_lang_info(self):
        """ test if tryChangeDetails works """
        self.assertEqual(len(self.lang_cache.get_changes()), 0)
        # create LanguageInformation object and test basic properties
        li = LanguageInformation(self.lang_cache, "de", "german")
        self.assertFalse(li.languagePkgList["languagePack"].installed)
        self.assertTrue(li.languagePkgList["languagePack"].available)

    def test_try_change_details(self):
        li = LanguageInformation(self.lang_cache, "de", "german")
        # test if writing aids get installed 
        li.languagePkgList["languagePack"].doChange = True
        self.lang_cache.tryChangeDetails(li)
        self.assertTrue(self.lang_cache["language-pack-de"].marked_install)
        self.assertTrue(self.lang_cache["hyphen-de"].marked_install)

if __name__ == "__main__":
    apt_pkg.config.set("Apt::Architecture","i386")
    unittest.main()
