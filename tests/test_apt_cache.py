#!/usr/bin/python3
#
# Copyright (C) 2010 Julian Andres Klode <jak@debian.org>
#               2010 Michael Vogt <mvo@ubuntu.com>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
"""Unit tests for verifying the correctness of check_dep, etc in apt_pkg."""

import glob
import logging
import os
import shutil
import sys
import tempfile
import unittest

from test_all import get_library_dir

libdir = get_library_dir()
if libdir:
    sys.path.insert(0, libdir)

import apt_pkg
import testcommon

import apt


def if_sources_list_is_readable(f):
    def wrapper(*args, **kwargs):
        if os.access("/etc/apt/sources.list", os.R_OK):
            f(*args, **kwargs)
        else:
            logging.warning("skipping '%s' because sources.list is not readable" % f)

    return wrapper


def get_open_file_descriptors():
    try:
        fds = os.listdir("/proc/self/fd")
    except OSError:
        logging.warning("failed to list /proc/self/fd")
        return set()
    return set(map(int, fds))


class TestAptCache(testcommon.TestCase):
    """test the apt cache"""

    def setUp(self):
        testcommon.TestCase.setUp(self)
        apt_pkg.config.clear("APT::Update::Post-Invoke")
        apt_pkg.config.clear("APT::Update::Post-Invoke-Success")

    @if_sources_list_is_readable
    def test_apt_cache(self):
        """cache: iterate all packages and all dependencies"""
        cache = apt.Cache()
        # number is not meaningful and just need to be "big enough",
        # the important bit is the test against __len__
        self.assertTrue(len(cache) > 100)
        # go over the cache and all dependencies, just to see if
        # that is possible and does not crash
        for pkg in cache:
            if pkg.candidate:
                for or_deps in pkg.candidate.dependencies:
                    for dep in or_deps:
                        self.assertTrue(dep.name)
                        self.assertTrue(isinstance(dep.relation, str))
                        self.assertTrue(dep.pre_depend in (True, False))

                # accessing record should take a reasonable time; in
                # particular, when using compressed indexes, it should not use
                # tons of seek operations
                r = pkg.candidate.record
                self.assertEqual(r["Package"], pkg.shortname)
                self.assertTrue("Version" in r)
                self.assertTrue(len(r["Description"]) > 0)
                self.assertTrue(str(r).startswith("Package: %s\n" % pkg.shortname))

    @if_sources_list_is_readable
    def test_cache_close_leak_fd(self):
        fds_before_open = get_open_file_descriptors()
        cache = apt.Cache()
        opened_fd = get_open_file_descriptors().difference(fds_before_open)
        cache.close()
        fds_after_close = get_open_file_descriptors()
        unclosed_fd = opened_fd.intersection(fds_after_close)
        self.assertEqual(fds_before_open, fds_after_close)
        self.assertEqual(unclosed_fd, set())

    def test_cache_open_twice_leaks_fds(self):
        cache = apt.Cache()
        fds_before_open = get_open_file_descriptors()
        cache.open()
        fds_after_open_twice = get_open_file_descriptors()
        self.assertEqual(fds_before_open, fds_after_open_twice)

    @if_sources_list_is_readable
    def test_cache_close_download_fails(self):
        cache = apt.Cache()
        self.assertEqual(cache.required_download, 0)
        cache.close()
        with self.assertRaises(apt.cache.CacheClosedException):
            cache.required_download

    def test_get_provided_packages(self):
        apt.apt_pkg.config.set("Apt::architecture", "i386")
        cache = apt.Cache(rootdir="./data/test-provides/")
        cache.open()
        if len(cache) == 0:
            logging.warning("skipping test_get_provided_packages, cache empty?!?")
            return
        # a true virtual pkg
        li = cache.get_providing_packages("mail-transport-agent")
        self.assertTrue(len(li) > 0)
        self.assertTrue("postfix" in [p.name for p in li])
        self.assertTrue("mail-transport-agent" in cache["postfix"].candidate.provides)

    def test_low_level_pkg_provides(self):
        apt.apt_pkg.config.set("Apt::architecture", "i386")
        # create highlevel cache and get the lowlevel one from it
        highlevel_cache = apt.Cache(rootdir="./data/test-provides")
        if len(highlevel_cache) == 0:
            logging.warning("skipping test_log_level_pkg_provides, cache empty?!?")
            return
        # low level cache provides list of the pkg
        cache = highlevel_cache._cache
        li = cache["mail-transport-agent"].provides_list
        # arbitrary number, just needs to be higher enough
        self.assertEqual(len(li), 2)
        for providesname, providesver, version in li:
            self.assertEqual(providesname, "mail-transport-agent")
            if version.parent_pkg.name == "postfix":
                break
        else:
            self.assertNotReached()

    @if_sources_list_is_readable
    def test_dpkg_journal_dirty(self):
        # create tmp env
        tmpdir = tempfile.mkdtemp()
        dpkg_dir = os.path.join(tmpdir, "var", "lib", "dpkg")
        os.makedirs(os.path.join(dpkg_dir, "updates"))
        open(os.path.join(dpkg_dir, "status"), "w").close()
        apt_pkg.config.set("Dir::State::status", os.path.join(dpkg_dir, "status"))
        cache = apt.Cache()
        # test empty
        self.assertFalse(cache.dpkg_journal_dirty)
        # that is ok, only [0-9] are dpkg jounral entries
        open(os.path.join(dpkg_dir, "updates", "xxx"), "w").close()
        self.assertFalse(cache.dpkg_journal_dirty)
        # that is a dirty journal
        open(os.path.join(dpkg_dir, "updates", "000"), "w").close()
        self.assertTrue(cache.dpkg_journal_dirty)

    @if_sources_list_is_readable
    def test_apt_update(self):
        rootdir = "./data/tmp"
        if os.path.exists(rootdir):
            shutil.rmtree(rootdir)
        try:
            os.makedirs(os.path.join(rootdir, "var/lib/apt/lists/partial"))
        except OSError:
            pass
        state_dir = os.path.join(rootdir, "var/lib/apt")
        lists_dir = os.path.join(rootdir, "var/lib/apt/lists")
        old_state = apt_pkg.config.find("dir::state")
        apt_pkg.config.set("dir::state", state_dir)
        # set a local sources.list that does not need the network
        base_sources = os.path.abspath(os.path.join(rootdir, "sources.list"))
        old_source_list = apt_pkg.config.find("dir::etc::sourcelist")
        old_source_parts = apt_pkg.config.find("dir::etc::sourceparts")
        apt_pkg.config.set("dir::etc::sourcelist", base_sources)
        # TODO: /dev/null is not a dir, perhaps find something better
        apt_pkg.config.set("dir::etc::sourceparts", "/dev/null")
        # main sources.list
        sources_list = base_sources
        with open(sources_list, "w") as f:
            repo = os.path.abspath("./data/test-repo2")
            f.write("deb [allow-insecure=yes] copy:%s /\n" % repo)

        # test single sources.list fetching
        sources_list = os.path.join(rootdir, "test.list")
        with open(sources_list, "w") as f:
            repo_dir = os.path.abspath("./data/test-repo")
            f.write("deb [allow-insecure=yes] copy:%s /\n" % repo_dir)

        self.assertTrue(os.path.exists(sources_list))
        # write marker to ensure listcleaner is not run
        open("./data/tmp/var/lib/apt/lists/marker", "w").close()

        # update a single sources.list
        cache = apt.Cache()
        cache.update(sources_list=sources_list)
        # verify we just got the excpected package file
        needle_packages = glob.glob(lists_dir + "/*tests_data_test-repo_Packages*")
        self.assertEqual(len(needle_packages), 1)
        # verify that we *only* got the Packages file from a single source
        all_packages = glob.glob(lists_dir + "/*_Packages*")
        self.assertEqual(needle_packages, all_packages)
        # verify that the listcleaner was not run and the marker file is
        # still there
        self.assertTrue("marker" in os.listdir(lists_dir))
        # now run update again (without the "normal" sources.list that
        # contains test-repo2 and verify that we got the normal sources.list
        cache.update()
        needle_packages = glob.glob(lists_dir + "/*tests_data_test-repo2_Packages*")
        self.assertEqual(len(needle_packages), 1)
        all_packages = glob.glob(lists_dir + "/*_Packages*")
        self.assertEqual(needle_packages, all_packages)

        # and another update with a single source only
        cache = apt.Cache()
        cache.update(sources_list=sources_list)
        all_packages = glob.glob(lists_dir + "/*_Packages*")
        self.assertEqual(len(all_packages), 2)
        apt_pkg.config.set("dir::state", old_state)
        apt_pkg.config.set("dir::etc::sourcelist", old_source_list)
        apt_pkg.config.set("dir::etc::sourceparts", old_source_parts)

    def test_package_cmp(self):
        cache = apt.Cache(rootdir="/")
        li = []
        li.append(cache["intltool"])
        li.append(cache["python3"])
        li.append(cache["apt"])
        li.sort()
        self.assertEqual([p.name for p in li], ["apt", "intltool", "python3"])

    def test_get_architectures(self):
        main_arch = apt.apt_pkg.config.get("APT::Architecture")
        arches = apt_pkg.get_architectures()
        self.assertTrue(main_arch in arches)

    def test_phasing_applied(self):
        """checks the return type of phasing_applied."""
        cache = apt.Cache()
        pkg = cache["apt"]
        self.assertIsInstance(pkg.phasing_applied, bool)

    def test_is_security_update(self):
        """checks the return type of is_security_update."""
        cache = apt.Cache()
        pkg = cache["apt"]
        self.assertIsInstance(pkg.installed.is_security_update, bool)

    def test_apt_cache_reopen_is_safe(self):
        """cache: check that we cannot use old package objects after reopen"""
        cache = apt.Cache()
        old_depcache = cache._depcache
        old_package = cache["apt"]
        old_pkg = old_package._pkg
        old_version = old_package.candidate
        old_ver = old_version._cand

        cache.open()
        new_depcache = cache._depcache
        new_package = cache["apt"]
        new_pkg = new_package._pkg
        new_version = new_package.candidate
        new_ver = new_version._cand

        # get candidate
        self.assertRaises(ValueError, old_depcache.get_candidate_ver, new_pkg)
        self.assertRaises(ValueError, new_depcache.get_candidate_ver, old_pkg)
        self.assertEqual(new_ver, new_depcache.get_candidate_ver(new_pkg))
        self.assertEqual(old_package.candidate._cand, old_ver)  # Remap success
        self.assertEqual(
            old_package.candidate._cand, new_depcache.get_candidate_ver(new_pkg)
        )

        # set candidate
        new_package.candidate = old_version
        old_depcache.set_candidate_ver(old_pkg, old_ver)
        self.assertRaises(ValueError, old_depcache.set_candidate_ver, old_pkg, new_ver)
        self.assertRaises(ValueError, new_depcache.set_candidate_ver, old_pkg, old_ver)
        self.assertRaises(ValueError, new_depcache.set_candidate_ver, old_pkg, new_ver)
        self.assertRaises(ValueError, new_depcache.set_candidate_ver, new_pkg, old_ver)
        new_depcache.set_candidate_ver(new_pkg, new_ver)

    @staticmethod
    def write_status_file(packages):
        with open(apt_pkg.config["Dir::State::Status"], "w") as fobj:
            for package in packages:
                print("Package:", package, file=fobj)
                print("Status: install ok installed", file=fobj)
                print("Priority: optional", file=fobj)
                print("Section: admin", file=fobj)
                print("Installed-Size: 1", file=fobj)
                print("Maintainer: X <x@x.invalid>", file=fobj)
                print("Architecture: all", file=fobj)
                print("Version: 1", file=fobj)
                print("Description: blah", file=fobj)
                print("", file=fobj)

    def test_apt_cache_reopen_is_safe_out_of_bounds(self):
        """Check that out of bounds access is remapped correctly."""
        with tempfile.NamedTemporaryFile() as status:
            apt_pkg.config["Dir::Etc::SourceList"] = "/dev/null"
            apt_pkg.config["Dir::Etc::SourceParts"] = "/dev/null"
            apt_pkg.config["Dir::State::Status"] = status.name
            apt_pkg.init_system()

            self.write_status_file("abcdefghijklmnopqrstuvwxyz")
            c = apt.Cache()
            p = c["z"]
            p_id = p.id
            self.write_status_file("az")
            apt_pkg.init_system()
            c.open()
            self.assertNotEqual(p.id, p_id)
            self.assertLess(p.id, 2)
            p.mark_delete()
            self.assertEqual([p], c.get_changes())

    def test_apt_cache_reopen_is_safe_out_of_bounds_no_match(self):
        """Check that installing gone package raises correct exception"""
        with tempfile.NamedTemporaryFile() as status:
            apt_pkg.config["Dir::Etc::SourceList"] = "/dev/null"
            apt_pkg.config["Dir::Etc::SourceParts"] = "/dev/null"
            apt_pkg.config["Dir::State::Status"] = status.name
            apt_pkg.init_system()

            self.write_status_file("abcdefghijklmnopqrstuvwxyz")
            c = apt.Cache()
            p = c["z"]
            p_id = p.id
            self.write_status_file("a")
            apt_pkg.init_system()
            c.open()
            self.assertEqual(p.id, p_id)  # Could not be remapped
            self.assertRaises(apt_pkg.CacheMismatchError, p.mark_delete)

    def test_apt_cache_reopen_is_safe_swap(self):
        """Check that swapping a and b does not mark the wrong package."""
        with tempfile.NamedTemporaryFile() as status:
            apt_pkg.config["Dir::Etc::SourceList"] = "/dev/null"
            apt_pkg.config["Dir::Etc::SourceParts"] = "/dev/null"
            apt_pkg.config["Dir::State::Status"] = status.name
            apt_pkg.init_system()

            self.write_status_file("abcdefghijklmnopqrstuvwxyz")
            c = apt.Cache()
            p = c["a"]
            a_id = p.id
            p_hash = hash(p)
            set_of_p = {p}
            self.write_status_file("baz")
            apt_pkg.init_system()
            c.open()
            # b now has the same id as a in the old cache
            self.assertEqual(c["b"].id, a_id)
            self.assertNotEqual(p.id, a_id)
            # Marking a should still mark a, not b.
            p.mark_delete()
            self.assertEqual([p], c.get_changes())

            # Ensure that p can still be found in a set of it, as a test
            # for bug https://bugs.launchpad.net/bugs/1780099
            self.assertEqual(hash(p), p_hash)
            self.assertIn(p, set_of_p)

    def test_apt_cache_iteration_safe(self):
        """Check that iterating does not produce different results.

        This failed in 1.7.0~alpha2, because one part of the code
        looked up packages in the weak dict using the pretty name,
        and the other using the full name."""

        with tempfile.NamedTemporaryFile() as status:
            apt_pkg.config["Dir::Etc::SourceList"] = "/dev/null"
            apt_pkg.config["Dir::Etc::SourceParts"] = "/dev/null"
            apt_pkg.config["Dir::State::Status"] = status.name
            apt_pkg.init_system()

            self.write_status_file("abcdefghijklmnopqrstuvwxyz")

            c = apt.Cache()
            c["a"].mark_delete()
            self.assertEqual([c["a"]], [p for p in c if p.marked_delete])

    def test_problemresolver_keep_phased_updates(self):
        """Check that the c++ function can be called."""
        with tempfile.NamedTemporaryFile() as status:
            apt_pkg.config["Dir::Etc::SourceList"] = "/dev/null"
            apt_pkg.config["Dir::Etc::SourceParts"] = "/dev/null"
            apt_pkg.config["Dir::State::Status"] = status.name
            apt_pkg.init_system()

            cache = apt.Cache()
            problemresolver = apt.ProblemResolver(cache)
            self.assertIsNone(problemresolver.keep_phased_updates())


if __name__ == "__main__":
    unittest.main()
