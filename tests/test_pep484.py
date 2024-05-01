#!/usr/bin/python3

import os
import subprocess
import unittest


def hasMyPy():
    try:
        subprocess.check_call(["mypy", "--version"])
    except Exception:
        return False
    return True


class PackagePep484TestCase(unittest.TestCase):
    @unittest.skipIf(not hasMyPy(), "no mypy available")
    def test_pep484_clean(self):
        # FIXME: check all of it
        top_src_dir = os.path.join(os.path.dirname(__file__), "..", "apt")
        os.environ["MYPYPATH"] = os.path.join(
            os.path.dirname(__file__), "..", "typehinting"
        )
        self.assertEqual(subprocess.call(["mypy", "--strict", top_src_dir]), 0)


if __name__ == "__main__":
    unittest.main()
