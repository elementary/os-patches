#!/usr/bin/env python3

import setuptools
setuptools  # pyflakes
from DistUtilsExtra.auto import setup


setup(name='software-properties',
      version='0.60',
      packages=[
                'softwareproperties',
                'softwareproperties.dbus',
                'softwareproperties.gtk',
                'softwareproperties.kde',
                ],
      scripts=[
               'software-properties-gtk',
               'software-properties-kde',
               'add-apt-repository',
               ],
      data_files=[
                  ('lib/software-properties/',
                   ['software-properties-dbus', ]
                  ),
                  ],
      test_suite="tests",
     )
