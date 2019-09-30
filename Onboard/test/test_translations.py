#!/usr/bin/python3

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Author: marmuta <marmvta@gmail.com>
#

import sys
import os
import subprocess
import re
import tempfile
from glob import glob
import gettext
import unittest


class TestTranslations(unittest.TestCase):

    def setUp(self):
        """ create mo files so we can read them with gettext later """
        self.tmp_dir = tempfile.TemporaryDirectory(prefix="test_onboard_")
        localedir = self.tmp_dir.name

        files = glob("po/*.po")
        if not files:
            Exception("No po files found, aborting")
        for file in files:
            name = os.path.splitext(os.path.basename(file))[0]
            dstfile = os.path.join(localedir, name, "LC_MESSAGES", "onboard.mo")
            os.makedirs(os.path.dirname(dstfile))
            subprocess.call(["msgfmt", file, "-o", dstfile])

    #@unittest.expectedFailure
    def test_field_names(self):
        """ compare field names of all msgids and msgstrs for all languages """
        localedir = self.tmp_dir.name
        files = glob(os.path.join(localedir, "*", "LC_MESSAGES", "onboard.mo"))
        langs = [file.split(os.path.sep)[-3] for file in files]
        totalstrcount = 0
        strcount = 0
        totallangcount = 0
        langcount = 0
        msg = ""

        for lang in langs:
            trans = gettext.translation("onboard", localedir, [lang])
            pattern = re.compile("{[^}]*}")
            count = 0
            for msgid, msgstr in trans._catalog.items():
                idfields = pattern.findall(msgid)
                strfields = pattern.findall(msgstr)
                if idfields != strfields:
                    msg += "\nField mismatch: Lang={} msgid='{}' msgstr='{}'" \
                           .format(lang, msgid, msgstr)
                    count += 1
                totalstrcount += 1
            totallangcount += 1

            if count:
                langcount += 1
            strcount += count

        self.assertEqual(strcount, 0, "{}/{} strings in {}/{} language(s) "
                                      "with non-matching field names\n"
                                      "{}" \
                                      .format(strcount, totalstrcount,
                                              langcount, totallangcount, msg))

