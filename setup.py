#!/usr/bin/python3

from setuptools import setup
from DistUtilsExtra.command import (build_extra, build_i18n, build_help,
                                    build_icons)
import os
import sys

setup(name='language-selector',
      version='0.1',
      py_modules = ['language_support_pkgs'],
      packages=['LanguageSelector',
                'LanguageSelector.gtk'],
      scripts=['gnome-language-selector',
               'check-language-support'],
      data_files=[('share/language-selector/data',
                   ["data/language-selector.png",
                    "data/languagelist",
                    "data/langcode2locale",
                    "data/locale2langpack",
                    "data/pkg_depends",
                    "data/variants",
                    "data/LanguageSelector.ui"]),
                  # dbus stuff
                  ('share/dbus-1/system-services',
                   ['dbus_backend/com.ubuntu.LanguageSelector.service']),
                  ('../etc/dbus-1/system.d/',
                   ["dbus_backend/com.ubuntu.LanguageSelector.conf"]),
                  ('lib/language-selector/',
                   ["dbus_backend/ls-dbus-backend"]),
                  # pretty pictures
                  ('share/pixmaps',
                   ["data/language-selector.png"]),
                  # session init config file
                  ('../etc/profile.d',
                   ["data/cedilla-portuguese.sh"]),
                  ],
      entry_points='''[aptdaemon.plugins]
modify_cache_after=language_support_pkgs:apt_cache_add_language_packs
[packagekit.apt.plugins]
what_provides=language_support_pkgs:packagekit_what_provides_locale
''',
      cmdclass={"build": build_extra.build_extra,
                "build_i18n": build_i18n.build_i18n,
                "build_help": build_help.build_help,
                "build_icons": build_icons.build_icons,
                },

      )

