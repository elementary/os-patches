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
import time
import glob
import shutil
import tempfile
import subprocess
import unittest
from contextlib import contextmanager


class TestMigration(unittest.TestCase):

    def setUp(self):
        self._tmp_dir = tempfile.TemporaryDirectory(prefix="test_onboard_")
        self._dir = self._tmp_dir.name
        self._user_dir = os.path.join(self._dir, "onboard")
        self._model_dir = os.path.join(self._user_dir, "models")

    def test_no_models(self):
        os.mkdir(os.path.join(self._dir, "onboard")) # foil user dir migration
        with self._run_onboard() as p:
            self.assertEqual([],
                             self._get_model_files())

    def test_migrate_user_model(self):
        tests = [
            [
                # old user.lm becomes new model of system language
                [
                    ['user.lm', 1],
                ],
                [
                    ['en_US.lm', 1],
                ]
            ],
            [
                # a backup model is renamed too
                [
                    ['user.lm', 1],
                    ['user.lm.bak', 2],
                ],
                [
                    ['en_US.lm', 1],
                    ['en_US.lm.bak', 2],
                ]
            ],
            [
                # a backup alone is ignored
                [
                    ['user.lm.bak', 2],
                ],
                [
                    ['user.lm.bak', 2],
                ]
            ],
            [
                # must not overwrite existing files
                [
                    ['user.lm', 1],
                    ['user.lm.bak', 2],
                    ['en_US.lm', 3],
                    ['en_US.lm.bak', 4],
                ],
                [
                    ['en_US.lm', 3],
                    ['en_US.lm.bak', 4],
                    ['user.lm', 1],
                    ['user.lm.bak', 2],
                ]
            ],
            [
                # must not overwrite existing backup model
                [
                    ['user.lm', 1],
                    ['user.lm.bak', 2],
                    ['en_US.lm.bak', 4],
                ],
                [
                    ['en_US.lm', 1],
                    ['en_US.lm.bak', 4],
                    ['user.lm.bak', 2],
                ]
            ],
            [
                # must not overwrite existing model
                [
                    ['user.lm', 1],
                    ['user.lm.bak', 2],
                    ['en_US.lm', 3],
                ],
                [
                    ['en_US.lm', 3],
                    ['user.lm', 1],
                    ['user.lm.bak', 2],
                ]
            ],
        ]

        os.mkdir(self._user_dir)    # foil user dir migration
        os.mkdir(self._model_dir)

        for i, (_input, _output) in enumerate(tests):
            for fn, size in _input:
                self._touch(os.path.join(self._model_dir, fn), size)

            with self._run_onboard() as p:
                self.assertEqual(_output,
                                 self._get_model_files(), "test " + str(i))

    def _get_model_files(self):
        results = []
        for f in sorted(f for f in \
                        glob.glob(os.path.join(self._model_dir, "*"))):
            results.append([os.path.basename(f), os.path.getsize(f)])
            os.remove(f)
        return results

    @contextmanager
    def _run_onboard(self):
        try:
            env = dict(os.environ)
            env["XDG_DATA_HOME"] = self._dir
            env["LANG"] = "en_US.UTF-8"

            p = subprocess.Popen(["./onboard"], env=env)
            time.sleep(1)
            yield p

        finally:
            p.terminate()
            p.wait()

    @staticmethod
    def _touch(fn, size):
        with open(fn, mode="w") as f:
            if size:
                f.write("*"*size)

