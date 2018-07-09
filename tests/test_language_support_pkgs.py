#!/usr/bin/python3
# coding: UTF-8

import unittest
import os.path
import os
import sys
import apt
import tempfile
import shutil
import resource
import copy

srcdir = os.path.dirname(os.path.abspath(os.path.dirname(__file__)))
sys.path.insert(0, srcdir)
from language_support_pkgs import LanguageSupport, DEFAULT_DEPENDS_FILE, apt_cache_add_language_packs

class T(unittest.TestCase):
    @classmethod
    def setUpClass(klass):
        # initialize this once for better performance
        klass.apt_cache = apt.Cache()
        klass.pkg_depends = os.path.join(srcdir, 'data', 'pkg_depends')

    def setUp(self):
        # in case tests modify $PATH, save it here and restore it in tearDown
        self.orig_path = os.getenv('PATH')
        self.workdir = tempfile.mkdtemp()

    def tearDown(self):
        if self.orig_path:
            os.environ['PATH'] = self.orig_path
        shutil.rmtree(self.workdir)

    @unittest.skipUnless(os.path.exists(DEFAULT_DEPENDS_FILE), 
            'no system installed pkg_depends file')
    def test_parse_pkg_depends_system(self):
        '''Parse system-installed pkg_depends file'''

        ls = LanguageSupport(self.apt_cache)
        self.assertGreater(len(ls.pkg_depends), 5)

    def test_parse_pkg_depends_local(self):
        '''Parse pkg_depends file in source tree'''

        ls = LanguageSupport(self.apt_cache, self.pkg_depends)
        self.assertGreater(len(ls.pkg_depends), 5)
        self.assertTrue('de' in ls.pkg_depends[''])
        self.assertTrue('' in ls.pkg_depends['firefox'])
        self.assertTrue('tr' in ls.pkg_depends['firefox'][''])
        self.assertTrue('wa' in ls.pkg_depends['']['de'])

    def test_langcode_from_locale(self):
        '''_langcode_from_locale()'''

        self.assertEqual(LanguageSupport._langcode_from_locale('de'), 'de')
        self.assertEqual(LanguageSupport._langcode_from_locale('de_DE.UTF-8'), 'de')
        self.assertEqual(LanguageSupport._langcode_from_locale('en_GB'), 'en')
        self.assertEqual(LanguageSupport._langcode_from_locale('be_BY@latin'), 'be')
        self.assertEqual(LanguageSupport._langcode_from_locale('zh_CN.UTF-8'), 'zh-hans')
        self.assertEqual(LanguageSupport._langcode_from_locale('zh_TW'), 'zh-hant')

    def test_by_package_and_locale_trigger(self):
        '''by_package_and_locale() for a trigger package'''

        ls = LanguageSupport(self.apt_cache, self.pkg_depends)

        result = ls.by_package_and_locale('libreoffice-common', 'es_ES.UTF-8', True)
        self._check_valid_pkgs(result)
        # implicit locale suffix
        self.assertTrue('libreoffice-l10n-es' in result)
        self.assertTrue('libreoffice-help-es' in result)
        # explicit entry for that language
        self.assertTrue('myspell-es' in result)

        # language only
        result = ls.by_package_and_locale('libreoffice-common', 'de', True)
        self._check_valid_pkgs(result)
        self.assertTrue('libreoffice-l10n-de' in result)
        self.assertTrue('libreoffice-help-de' in result)

        # Chinese special case
        result = ls.by_package_and_locale('firefox', 'zh_CN.UTF-8', True)
        self._check_valid_pkgs(result)
        self.assertTrue('firefox-locale-zh-hans' in result)
        # no generic packages
        self.assertFalse('language-pack-zh-hans' in result)

        result = ls.by_package_and_locale('libreoffice-common', 'zh_CN.UTF-8', True)
        self._check_valid_pkgs(result)
        self.assertTrue('libreoffice-l10n-zh-cn' in result)
        self.assertTrue('libreoffice-help-zh-cn' in result)

        # without locale suffix
        result = ls.by_package_and_locale('chromium-browser', 'dv_MV', True)
        self._check_valid_pkgs(result)
        self.assertTrue('chromium-browser-l10n' in result)

    def test_by_package_and_locale_generic(self):
        '''by_package_and_locale() for generic support'''

        ls = LanguageSupport(self.apt_cache, self.pkg_depends)

        result = ls.by_package_and_locale('', 'en_GB.UTF-8', True)
        self._check_valid_pkgs(result)
        self.assertTrue('language-pack-en' in result)
        self.assertTrue('wbritish' in result)

        # language code only
        result = ls.by_package_and_locale('', 'de', True)
        self._check_valid_pkgs(result)
        self.assertTrue('language-pack-de' in result)
        self.assertTrue('wngerman' in result)

    def test_by_package_and_locale_noinstalled(self):
        '''by_package_and_locale() without installed packages'''

        ls = self._fake_apt_language_support(['language-pack-en'],
                ['wbritish'])

        result = ls.by_package_and_locale('', 'en_GB.UTF-8', False)
        self.assertEqual(result, ['wbritish'])

        ls = self._fake_apt_language_support(['language-pack-en', 'firefox',
            'firefox-locale-en', 'libreoffice-common', 'gedit'], 
            ['wbritish', 'firefox-locale-uk', 'hunspell-en-ca',
             'hunspell-en-us', 'aspell-en'])
        result = ls.by_package_and_locale('', 'en_GB.UTF-8', False)
        self.assertEqual(result, ['wbritish'])
        result = ls.by_package_and_locale('firefox', 'en_GB.UTF-8', False)
        self.assertEqual(result, [])
        result = ls.by_package_and_locale('abiword', 'en_GB.UTF-8', False)
        self.assertEqual(result, ['aspell-en'])

        # if we specify a country, do not give us stuff for other countries
        result = ls.by_package_and_locale('libreoffice-common', 'en_GB.UTF-8', False)
        self.assertEqual(result, [])

        # if we specify just the language, give us stuff for all countries
        result = ls.by_package_and_locale('libreoffice-common', 'en', False)
        self.assertEqual(set(result), set(['hunspell-en-ca', 'hunspell-en-us']))

    def test_by_package_and_locale_unknown(self):
        '''by_package_and_locale() for unknown locales/triggers'''

        ls = LanguageSupport(self.apt_cache, self.pkg_depends)

        result = ls.by_package_and_locale('unknown_pkg', 'de_DE.UTF-8', True)
        self.assertEqual(result, [])
        result = ls.by_package_and_locale('firefox', 'bo_GUS', True)
        self.assertEqual(result, [])

    def test_by_locale(self):
        '''by_locale()'''

        ls = self._fake_apt_language_support(['firefox-locale-de',
            'language-pack-de', 'gvfs', 'libreoffice-common', 'firefox'],
            ['language-pack-gnome-de', 'language-pack-kde-de', 'wngerman',
            'wbritish', 'libreoffice-help-de'])
        result = ls.by_locale('de_DE.UTF-8', True)
        self.assertEqual(set(result), set(['firefox-locale-de', 'language-pack-de',
            'language-pack-gnome-de', 'wngerman', 'libreoffice-help-de']))

        # no duplicated items in result list
        self.assertEqual(sorted(set(result)), sorted(result))

    def test_by_locale_zh(self):
        '''by_locale() for Chinese'''

        ls = self._fake_apt_language_support(['libreoffice-common', 'firefox',
            'ibus'], ['libreoffice-l10n-zh-cn', 'libreoffice-l10n-zh-tw',
                'ibus-sunpinyin', 'ibus-chewing', 'firefox-locale-zh-hans',
                'firefox-locale-zh-hant'])

        result = set(ls.by_locale('zh_CN', True))
        self.assertEqual(result, set(['libreoffice-l10n-zh-cn',
            'ibus-sunpinyin', 'firefox-locale-zh-hans']))

        # accepts both variants of specifying the locale; this is not
        # originally supposed to work, but some programs assume it does
        result = set(ls.by_locale('zh-hans', True))
        self.assertEqual(result, set(['libreoffice-l10n-zh-cn',
            'ibus-sunpinyin', 'firefox-locale-zh-hans']))

        result = set(ls.by_locale('zh_TW', True))
        self.assertEqual(result, set(['libreoffice-l10n-zh-tw',
            'ibus-chewing', 'firefox-locale-zh-hant']))

        result = set(ls.by_locale('zh-hant', True))
        self.assertEqual(result, set(['libreoffice-l10n-zh-tw',
            'ibus-chewing', 'firefox-locale-zh-hant']))

    def test_by_locale_noinstalled(self):
        '''by_locale() without installed packages'''

        ls = self._fake_apt_language_support(['firefox-locale-de',
            'language-pack-de', 'gvfs', 'libreoffice-common', 'firefox'],
            ['language-pack-gnome-de', 'language-pack-kde-de', 'wngerman',
            'wbritish', 'libreoffice-help-de'])
        result = ls.by_locale('de_DE.UTF-8', False)
        self.assertEqual(set(result), set(['language-pack-gnome-de', 'wngerman',
            'libreoffice-help-de']))

    def test_hunspell_de_frami(self):
        '''hunspell-de-frami special case'''

        # if neither is installed, suggest the hunspell-de-de default
        ls = self._fake_apt_language_support(['libreoffice-common'],
                                             ['hunspell-de-de', 'hunspell-de-de-frami'])
        self.assertEqual(ls.missing(), set(['hunspell-de-de']))

        # if the default is installed, it's complete
        ls = self._fake_apt_language_support(['libreoffice-common', 'hunspell-de-de'],
                                             ['hunspell-de-de-frami'])
        self.assertEqual(ls.missing(), set())

        # -frami also suffices
        ls = self._fake_apt_language_support(['libreoffice-common', 'hunspell-de-de-frami'],
                                             ['hunspell-de-de'])
        self.assertEqual(ls.missing(), set())

    def test_by_package(self):
        '''by_package()'''

        # create a fake locales -a
        fake_locale = os.path.join(self.workdir, 'locale')
        with open(fake_locale, 'w') as f:
            f.write('''#!/bin/sh
cat <<EOF
de_DE.UTF-8
pt_BR
zh_CN.GB18030
EOF
''')
        os.chmod(fake_locale, 0o755)
        os.environ['PATH'] = '%s:%s' % (self.workdir, os.getenv('PATH', ''))

        ls = self._fake_apt_language_support([], ['language-pack-de',
            'libreoffice-common', 'libreoffice-help-de',
            'libreoffice-l10n-pt-br', 'libreoffice-l10n-zh-cn',
            'libreoffice-l10n-pt', 'libreoffice-l10n-zh-tw',
            'libreoffice-l10n-uk'])
        result = ls.by_package('libreoffice-common', False)
        self.assertEqual(set(result), set(['libreoffice-help-de',
            'libreoffice-l10n-pt-br', 'libreoffice-l10n-zh-cn',
            'libreoffice-l10n-pt']))

        # no duplicated items in result list
        self.assertEqual(sorted(set(result)), sorted(result))

    def test_all(self):
        '''all()'''

        # create a fake locales -a
        fake_locale = os.path.join(self.workdir, 'locale')
        with open(fake_locale, 'w') as f:
            f.write('''#!/bin/sh
cat <<EOF
de_DE.UTF-8
pt_BR
EOF
''')
        os.chmod(fake_locale, 0o755)
        os.environ['PATH'] = '%s:%s' % (self.workdir, os.getenv('PATH', ''))

        ls = self._fake_apt_language_support(['libreoffice-common', 'firefox',
            'language-pack-de'], 
            ['libreoffice-help-de', 'libreoffice-l10n-pt-br', 'libreoffice-l10n-pt',
            'firefox-locale-de', 'firefox-locale-pt'])
        result = ls.missing(False)
        self.assertEqual(set(result), set(['libreoffice-help-de',
            'libreoffice-l10n-pt-br', 'libreoffice-l10n-pt',
            'firefox-locale-de', 'firefox-locale-pt']))

        # now with already installed packages
        result = ls.missing(True)
        self.assertEqual(set(result), set(['libreoffice-help-de',
            'libreoffice-l10n-pt-br', 'libreoffice-l10n-pt',
            'firefox-locale-de', 'firefox-locale-pt', 'language-pack-de']))

    def test_performance_of_missing(self):
        '''missing() performance for current system'''

        iterations = 20

        r_start = resource.getrusage(resource.RUSAGE_SELF)
        ls = LanguageSupport(self.apt_cache, self.pkg_depends)
        r_init = resource.getrusage(resource.RUSAGE_SELF)
        ls.missing(True)
        r_first = resource.getrusage(resource.RUSAGE_SELF)
        for i in range(iterations):
            ls.missing(True)
        r_iter = resource.getrusage(resource.RUSAGE_SELF)

        init = (r_init.ru_utime + r_init.ru_stime -
            r_start.ru_utime - r_start.ru_stime) * 1000000
        first = (r_first.ru_utime + r_first.ru_stime -
            r_init.ru_utime - r_init.ru_stime) * 1000000
        avg = (r_iter.ru_utime + r_iter.ru_stime -
            r_first.ru_utime - r_first.ru_stime) * 1000000/iterations
        sys.stderr.write('[%iμs init, %iμs first, %iμs avg] ' % (int(init+.5), int(first+.5), int(avg+.5)))

    def test_available_languages(self):
        '''available_languages()'''

        # create a fake locales -a
        fake_locale = os.path.join(self.workdir, 'locale')
        with open(fake_locale, 'w') as f:
            f.write('''#!/bin/sh
cat <<EOF
aa_ER@saaho
de_DE.UTF-8
en_AU.UTF-8
en_US.UTF-8
es_AR.UTF-8
pt_PT.UTF-8
pt_BR
ru_RU.UTF-8
zh_CN.GB18030
zh_TW.UTF-8
EOF
''')
        os.chmod(fake_locale, 0o755)
        os.environ['PATH'] = '%s:%s' % (self.workdir, os.getenv('PATH', ''))

        ls = LanguageSupport(self.apt_cache, self.pkg_depends)
        available = ls.available_languages()
        self.assertEqual(available, set(['aa', 'de', 'en', 'en_AU', 'en_US',
            'es', 'es_AR', 'pt', 'pt_BR', 'pt_PT', 'ru', 'zh_CN', 'zh_TW']))

    def test_countries_for_lang(self):
        '''_countries_for_lang()'''

        self.assertEqual(LanguageSupport._countries_for_lang('yue'), set(['hk']))
        # Serbia has @ variants
        self.assertEqual(LanguageSupport._countries_for_lang('sr'), set(['rs', 'me']))
        self.assertEqual(LanguageSupport._countries_for_lang('de'), 
                set(['at', 'be', 'ch', 'de', 'li', 'lu']))
        en = LanguageSupport._countries_for_lang('en')
        self.assertTrue('us' in en)
        self.assertTrue('gb' in en)
        self.assertTrue('sg' in en)

    def test_available_languages_system(self):
        '''available_languages() for system-installed locales'''

        # we cannot assume much here, just check that it works and does not
        # crash.
        ls = LanguageSupport(self.apt_cache, self.pkg_depends)
        available = ls.available_languages()
        self.assertGreater(len(available), 0)

    def test_apt_cache_add_language_packs(self):
        '''apt_cache_add_language_packs()'''

        # create a fake locales -a
        fake_locale = os.path.join(self.workdir, 'locale')
        with open(fake_locale, 'w') as f:
            f.write('''#!/bin/sh
cat <<EOF
de_DE.UTF-8
es_AR.UTF-8
EOF
''')
        os.chmod(fake_locale, 0o755)
        os.environ['PATH'] = '%s:%s' % (self.workdir, os.getenv('PATH', ''))

        # create a fake apt.Cache
        cache = self._fake_apt_language_support([], 
                ['firefox', 'firefox-locale-de', 'firefox-locale-es', 'firefox-locale-fi',
                'libreoffice-common', 'libreoffice-help-de',
                'language-pack-de', 'language-pack-gnome-de', 'language-pack-kde-de', 
                'gvfs', 'libreoffice-common',
                'wngerman', 'wbritish']).apt_cache

        self.assertEqual(cache.get_changes(), [])

        # if we do not have any changes, it should do nothing
        apt_cache_add_language_packs(None, cache)
        self.assertEqual(cache.get_changes(), [])

        # now install firefox
        c = copy.deepcopy(cache)
        c['firefox'].mark_install()
        apt_cache_add_language_packs(None, c, self.pkg_depends)
        new_pkgs = set([p.name for p in c.get_changes() if p.marked_install])
        # should not install the generic support, just firefox specific stuff
        self.assertEqual(new_pkgs, set(['firefox', 'firefox-locale-de', 'firefox-locale-es']))
        self.assertTrue(c['firefox'].marked_install_from_user)
        self.assertFalse(c['firefox-locale-de'].marked_install_from_user)
        self.assertFalse(c['firefox-locale-es'].marked_install_from_user)

        # install two packages
        c = copy.deepcopy(cache)
        c['gvfs'].mark_install()
        c['libreoffice-common'].mark_install()
        apt_cache_add_language_packs(None, c, self.pkg_depends)
        new_pkgs = set([p.name for p in c.get_changes() if p.marked_install])
        self.assertEqual(new_pkgs, set(['gvfs', 'libreoffice-common',
            'libreoffice-help-de', 'language-pack-gnome-de']))

    def _fake_apt_language_support(self, installed, available):
        '''Return a LanguageSupport object with a fake apt_cache.'''

        class FakeCache(dict):
            def get_changes(self):
                result = []
                for pkg in self.values():
                    if pkg.marked_install:
                        result.append(pkg)
                return result

        class Pkg:
            def __init__(self, name, installed):
                self.name = name
                self.installed = installed
                self.marked_install = False
                self.marked_install_from_user = False

            def __str__(self):
                if self.installed:
                    return '%s (installed)' % self.name
                else:
                    return '%s (available)' % self.name

            def __repr__(self):
                if self.installed:
                    return 'Pkg("%s", True)' % self.name
                else:
                    return 'Pkg("%s", False)' % self.name

            def mark_install(self, from_user=True):
                self.marked_install = True
                self.marked_install_from_user = from_user

        cache = FakeCache()
        for p in available:
            cache[p] = Pkg(p, False)
        for p in installed:
            cache[p] = Pkg(p, True)

        return LanguageSupport(cache, self.pkg_depends)

    def _check_valid_pkgs(self, packages):
        '''Check that all packages in a given list exist.'''

        for pkg in packages:
            self.assertTrue(pkg in self.apt_cache, 'package %s does not exist' % pkg)

unittest.main()
