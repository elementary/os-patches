"""Common testing stuff"""

import apt_pkg

import unittest


class TestCase(unittest.TestCase):
    """Base class for python-apt unittests"""

    def setUp(self):
        self.resetConfig()

    def resetConfig(self):
        apt_pkg.config.clear("")
        for key in apt_pkg.config.list():
            apt_pkg.config.clear(key)

        apt_pkg.init_config()
        apt_pkg.init_system()
