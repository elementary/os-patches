#!/usr/bin/python3

# Copyright © 2012, marmuta <marmvta@gmail.com>
#
# This file is part of Onboard.
#
# Onboard is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Onboard is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

import os
import shutil
import unittest
import tempfile
import subprocess
import Onboard.pypredict as pypredict


class TestCheckModels(unittest.TestCase):

    TOOL = "Onboard/pypredict/tools/checkmodels"
    MAX_ORDER = 5

    def __init__(self, *params):
        super(TestCheckModels, self).__init__(*params)

    def setUp(self):
        self._tmp_dir = tempfile.TemporaryDirectory(prefix="test_onboard_")
        self._dir = self._tmp_dir.name

        text = "word1 word2 word3 word4 word5"
        tokens, _spans = pypredict.tokenize_text(text)

        # prepare contents of error-free models
        self._model_contents = []
        self._models = []
        for i in range(0, self.MAX_ORDER):
            order = i + 1
            fn = os.path.join(self._dir, "order{}.lm".format(order))
            if order == 1:
                model = pypredict.UnigramModel()
            else:
                model = pypredict.DynamicModel(order)
            model.learn_tokens(tokens)
            model.save(fn)

            with open(fn, encoding="UTF-8") as f:
                lines = f.readlines()

            self._models.append(model)
            self._model_contents.append([fn, lines])

    def test_can_run_outside_source_tree(self):
        tool_name = os.path.basename(self.TOOL)
        fn = os.path.join(self._dir, "not-there.lm")
        for base_dir in [None,
                         os.path.join(os.path.expanduser("~"), ".cache"),
                         os.path.expanduser("~")]:
            error = None
            ret = out = err = None
            try:
                tmp_dir = tempfile.TemporaryDirectory(prefix="test_onboard_",
                                                      dir=base_dir)
                tmp_dir_name = tmp_dir.name
                tool = os.path.join(tmp_dir_name, tool_name)
                shutil.copyfile(self.TOOL, tool)
                os.chmod(tool, 0o544) # make it executable
                ret, out, err = self._run_tool(fn, tool)
                break
            except PermissionError as e:
                error = "cannot execute '{}': {}" \
                          .format(tool_name, e)
            except FileNotFoundError as e: #
                error = "cannot copy '{}': {}" \
                          .format(tool_name, e)
        if error:
            self.skipTest(error)
        self.assertEqual(['FILE_NOT_FOUND, []'], err)

    def test_file_not_found(self):
        fn = os.path.join(self._dir, "test.lm")
        ret, out, err = self._run_tool(fn)
        self.assertEqual(['FILE_NOT_FOUND, []'], err)
        self.assertTrue(ret != 0)

    def test_not_a_file(self):
        fn = os.path.join(self._dir, "dir.lm")
        os.mkdir(fn)
        ret, out, err = self._run_tool(fn)
        os.rmdir(fn)
        self.assertEqual(['NOT_A_FILE, []'], err)
        self.assertTrue(ret != 0)

    def test_empty_file(self):
        fn = os.path.join(self._dir, "test.lm")
        self._touch(fn)
        ret, out, err = self._run_tool(fn)
        self.assertEqual(['EMPTY_FILE, []'], err)
        self.assertTrue(ret != 0)

    def test_no_errors(self):
        for i, (fn, lines) in enumerate(self._model_contents):
            order = i+1
            ret, out, err = self._run_tool(fn)
            self.assertEqual([], err, "at order {}".format(order))
            self._test_model_info(out, order, lines)
            self.assertTrue(ret == 0, "at order {}".format(order))

    def test_no_data_section(self):
        for i, (fn, lines) in enumerate(self._model_contents):
            order = i+1
            nlines = []
            for line in lines:
                if "\\data\\" in line:
                    line = "data\n"
                nlines.append(line)
            self._write_contents(fn, nlines)
            ret, out, err = self._run_tool(fn)
            self.assertEqual(['NO_DATA_SECTION, []'], err,
                             "at order {}".format(order))
            self._test_model_info(out, order, nlines, [None]*order)

    def test_unexpected_eof(self):
        for i, (fn, lines) in enumerate(self._model_contents):
            order = i+1
            nlines = []
            for line in lines:
                if "\\end\\" in line:
                    line = "end"
                nlines.append(line)
            self._write_contents(fn, nlines)
            ret, out, err = self._run_tool(fn)
            err = [e for e in err if not "WRONG_NUMBER_OF_FIELDS" in e]
            expected = ['UNEXPECTED_EOF, [{}]'.format(nlines[-5:])]
            self.assertEqual(expected, err,
                             "at order {}".format(order))
            self._test_model_info(out, order, nlines)

    def test_empty_data_section(self):
        for i, (fn, lines) in enumerate(self._model_contents):
            order = i+1
            nlines = []
            for line in lines:
                if not "=" in line:
                    nlines.append(line)
            self._write_contents(fn, nlines)
            ret, out, err = self._run_tool(fn)
            err = [e for e in err if not "UNEXPECTED_NGRAM_SECTION" in e]
            self.assertEqual(['EMPTY_DATA_SECTION, []'], err,
                             "at order {}".format(order))

    def test_bad_data_section_entry(self):
        for entry in ["foo=123", "ngram 1=3a3", "bar", "gram 1=3"]:
            for i, (fn, lines) in enumerate(self._model_contents):
                order = i+1
                nlines = []
                for line in lines:
                    if "=" in line:
                        line = entry + "\n"
                    nlines.append(line)
                self._write_contents(fn, nlines)
                ret, out, err = self._run_tool(fn)
                err = [e for e in err if not "UNEXPECTED_NGRAM_SECTION" in e]
                expected = ["BAD_DATA_SECTION_ENTRY, ['"+entry+"']"]*order + \
                           ['EMPTY_DATA_SECTION, []']
                self.assertEqual(expected, err,
                    "entry '{}' at order {}".format(entry, order))

    def test_wrong_number_of_fields(self):
        for field_change in [-1, +1]:
            for i, (fn, lines) in enumerate(self._model_contents):
                order = i+1

                nlines = []
                changes = []
                count = None
                lineno = 0
                for line in lines:
                    lineno += 1
                    if "-grams:" in line:
                        count = 0
                    if count == 2:
                        count = None
                        fields = line.split()
                        n0 = len(fields)
                        n1 = n0 + field_change
                        if n0:
                            fields = (fields + ["extra"])[:n1]
                            line = " ".join(fields) + "\n"
                            changes.append([n1, n0, lineno, line.strip()])
                    if not count is None:
                        count += 1
                    nlines.append(line)

                self._write_contents(fn, nlines)
                ret, out, err = self._run_tool(fn)
                err = [e for e in err if not "WRONG_NGRAM_COUNT" in e]
                expected = []
                for change in changes:
                    e = "WRONG_NUMBER_OF_FIELDS, [{}, {}, {}, '{}']" \
                        .format(*change)
                    expected.append(e)
                self.assertEqual(expected, err,
                    "field_change '{}' at order {}".format(field_change, order))

    def test_unexpected_ngram_section(self):
        for i, (fn, lines) in enumerate(self._model_contents):
            order = i+1
            if order == 1:
                continue

            nlines = []
            count = None
            changes = []
            for line in lines:
                if "\\data\\" in line:
                    count = 0
                if not count is None:
                    if count >= 1 and count < order:
                        changes.append([count])
                        line = "\n"
                    count += 1
                nlines.append(line)

            self._write_contents(fn, nlines)
            ret, out, err = self._run_tool(fn)
            expected = []
            model = self._models[i]
            counts = model.get_counts()
            for change in changes:
                level = change[0]
                e = "UNEXPECTED_NGRAM_SECTION, [{}, {}]" \
                    .format(level, counts[0][level-1])
                expected.append(e)
            self.assertEqual(expected, err,
                "at order {}".format(order))

    def test_wrong_ngram_count(self):
        for i, (fn, lines) in enumerate(self._model_contents):
            order = i+1

            nlines = []
            count = None
            for line in lines:
                if "-grams:" in line:
                    count = 0
                if not count is None:
                    if count == 1:
                        line = "\n"
                        count = None
                if not count is None:
                    count += 1
                nlines.append(line)

            self._write_contents(fn, nlines)
            ret, out, err = self._run_tool(fn)
            expected = []
            model = self._models[i]
            counts = model.get_counts()
            for i in range(order):
                level = i + 1
                count = counts[0][level-1]
                e = "WRONG_NGRAM_COUNT, [{}, {}, {}]" \
                    .format(level, count, count-1)
                expected.append(e)
            self.assertEqual(expected, err,
                "at order {}".format(order))

    def _test_model_info(self, out, order, lines, data_counts = None):
        """
        Expects in stdout:
            file_size, num_lines
            level, data_count, encountered_count
            ...
        """
        fn, _lines = self._model_contents[order-1]
        model = self._models[order-1]
        counts = model.get_counts()
        expected = ["{}, {}".format(os.path.getsize(fn), len(lines))]
        for i in range(order):
            data_count = data_counts[i] if data_counts else counts[0][i]
            expected += ["{}, {}, {}".format(i+1, data_count, counts[0][i])]
        self.assertEqual(expected, out, "at order {}".format(order))


    def _run_tool(self, fn, tool=None):
        if tool is None:
            tool = self.TOOL
        p = subprocess.Popen([tool, "--test", fn],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        try:
            out, err = p.communicate(timeout=5)
        except TimeoutExpired:
            p.kill()
            out, err = p.communicate()
        return (p.returncode,
                out.decode("UTF-8").splitlines(),
                err.decode("UTF-8").splitlines())

    def _write_contents(self, fn, lines):
        with open(fn, mode="w", encoding="UTF-8") as f:
            for l in lines:
                f.write(l)

    @staticmethod
    def _touch(fn):
        with open(fn, mode="w") as f:
            pass


if __name__ == "__main__":
    unittest.main()
