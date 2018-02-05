#!/usr/bin/python
#
# Copyright (C) 2010 Michael Vogt <mvo@ubuntu.com>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of apt.progress"""
import unittest

import apt
import apt_pkg
import os

import testcommon


class TestAcquireProgress(apt.progress.base.AcquireProgress):
    def pulse(self, owner):
        self.pulsed = True
        # there should be a return value here, either (True,False)
        # but often this is forgoten (and causes odd error messages)
        # so the lib supports it. we test the lack of support value here


class TestProgress(testcommon.TestCase):

    def setUp(self):
        testcommon.TestCase.setUp(self)
        basedir = os.path.abspath(os.path.dirname(__file__))
        # setup apt_pkg config
        apt_pkg.init()
        apt_pkg.config.set("APT::Architecture", "amd64")
        apt_pkg.config.set("Dir::Etc", basedir)
        # TODO: /dev/null is not a dir, perhaps find something better
        apt_pkg.config.set("Dir::Etc::sourceparts", "/dev/null")
        # setup lists dir
        if not os.path.exists("./tmp/partial"):
            os.makedirs("./tmp/partial")
        apt_pkg.config.set("Dir::state::lists", "./tmp")
        # create artifical line
        deb_line = ("deb [allow-insecure=yes] file:%s/data/fake-packages/ /\n"
                    % basedir)
        with open("fetch_sources.list", "w") as fobj:
            fobj.write(deb_line)
        apt_pkg.config.set("Dir::Etc::sourcelist", "fetch_sources.list")
        apt_pkg.config.clear("APT::Update::Post-Invoke")
        apt_pkg.config.clear("APT::Update::Post-Invoke-Success")

    def test_acquire_progress(self):
        progress = TestAcquireProgress()
        cache = apt.Cache()
        cache.update(progress)
        self.assertTrue(progress.pulsed)


if __name__ == "__main__":
    unittest.main()
