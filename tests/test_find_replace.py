#!/usr/bin/python3

import unittest
import sys
sys.path.insert(0, "../")

from LanguageSelector.utils import *

class TestFindReplace(unittest.TestCase):

    def setUp(self):
        self.test_string="""xxx
line
search_text and more
line 17
"""

    def test_find_replace(self):
        with open("foo.txt","w") as f:
            f.write(self.test_string)
        find_string_and_replace("search_text", "lala", ["foo.txt"])
        with open("foo.txt") as f:
            new_content = f.read()
        self.assertFalse("search_text" in new_content)
        self.assertTrue("lala" in new_content)
        os.unlink("foo.txt")

    def test_find_replace_not_in_file(self):
        with open("foo.txt","w") as f:
            f.write(self.test_string)
        find_string_and_replace("string_not_in_file", "lala", ["foo.txt"])
        with open("foo.txt") as f:
            new_content = f.read()
        self.assertTrue("lala" in new_content)
        os.unlink("foo.txt")

if __name__ == "__main__":
    unittest.main()
