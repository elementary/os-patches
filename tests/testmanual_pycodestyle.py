import os
import subprocess
import unittest


class PackagePyCodeStyleTestCase(unittest.TestCase):

    def test_pycodestyle(self):
        res = 0
        py_dir = os.path.join(os.path.dirname(__file__), "..")
        res += subprocess.call(
            ["pycodestyle",
             # disable for now:
             # E125 continuation line does not distinguish itself from
             #      next logical line
             # E126 continuation line over-indented for hanging indent
             # E127 continuation line over-indented for visual indent
             # E128 continuation line under-indented for visual indent
             # E129 continuation line does not distinguish itself from
             #      next logical line
             # E265 block comment should start with '# '
             # E402 module level import not at top of file (breaks tests)
             # W504 line break after binary operator (that's the
             #      correct behavior)
             "--ignore=E125,E126,E127,E128,E129,E265,E402,W504",
             "--exclude", "build,tests/old",
             "--repeat", py_dir])
        if res != 0:
            self.fail("pycodestyle failed with: %s" % res)


if __name__ == "__main__":
    unittest.main()
