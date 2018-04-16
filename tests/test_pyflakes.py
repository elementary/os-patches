import glob
import os
import subprocess
import unittest

class TestPyflakesClean(unittest.TestCase):
    """ ensure that the tree is pyflakes clean """

    BLACKLIST = ("/softwareproperties/kde",
                )

    def setUp(self):
        self.paths = []
        basedir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        for dirpath, dirs, files in os.walk(basedir):
            if any([dirpath.endswith(p) for p in self.BLACKLIST]):
                continue
            self.paths.extend(glob.glob(dirpath+"/*.py"))

    def test_pyflakes3_clean(self):
        self.assertEqual(subprocess.check_call(['pyflakes3'] +  self.paths), 0)


if __name__ == "__main__":
    unittest.main()
