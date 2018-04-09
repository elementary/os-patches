#!/usr/bin/python
# -*- coding: utf-8 -*-

from __future__ import print_function

from gi.repository import GLib, Gio

import apt_pkg

import dbus
import logging
import glob
import os
import subprocess
import sys
import threading
import time
import unittest

from dbus.mainloop.glib import DBusGMainLoop

sys.path.insert(0, "../")
from softwareproperties.dbus.SoftwarePropertiesDBus import (
    SoftwarePropertiesDBus)
from softwareproperties import (
    UPDATE_INST_SEC, UPDATE_DOWNLOAD, UPDATE_NOTIFY)


def get_test_source_line():
    distro_release = get_distro_release()
    return "deb http://archive.ubuntu.com/ubuntu %s main restricted #"\
           " comment with unicode äöü" % distro_release


def get_distro_release():
    return subprocess.check_output(["lsb_release", "-c", "-s"], universal_newlines=True).strip()


def clear_apt_config():
    etc_apt = os.path.join(os.path.dirname(__file__), "aptroot", "etc", "apt")
    for dirpath, dirnames, filenames in os.walk(etc_apt):
        for name in filenames:
            path = os.path.join(dirpath, name)
            if os.path.isfile(path):
                os.unlink(path)


def create_sources_list():
    s = get_test_source_line() + "\n"
    name = os.path.join(os.path.dirname(__file__),
                        "aptroot", "etc", "apt", "sources.list")
    with open(name, "w") as f:
        f.write(s)
    return name


def _on_name_appeared(loop):
    def _real(connection, name, new_owner):
        loop.quit()
    return _real

# XXX: Find a way to do this without a global (e.g. rewrite with GDBus)
should_quit = False

def _on_timeout(loop):
    global should_quit
    if should_quit:
        loop.quit()
        return GLib.SOURCE_REMOVE

    return GLib.SOURCE_CONTINUE

def _start_software_properties_dbus_in_session_bus_thread():
    DBusGMainLoop(set_as_default=True)
    loop = GLib.MainLoop()
    threading.local().loop = loop

    GLib.timeout_add_seconds(1, _on_timeout, loop)

    # don't hang forever in case something goes wrong
    GLib.timeout_add_seconds(120, lambda l: l.quit(), loop)

    bus = dbus.SessionBus(private=True)

    rootdir = os.path.join(os.path.dirname(__file__), "aptroot")
    spd = SoftwarePropertiesDBus(bus, rootdir=rootdir)
    spd.enforce_polkit = False
    loop.run()

def start_software_properties_dbus_in_session_bus():
    loop = GLib.MainLoop()
    watch_id = Gio.bus_watch_name(Gio.BusType.SESSION,
                                  "com.ubuntu.SoftwareProperties",
                                  Gio.BusNameWatcherFlags.NONE,
                                  _on_name_appeared(loop),
                                  None)
    t = threading.Thread(target=_start_software_properties_dbus_in_session_bus_thread)
    t.start()
    loop.run()
    Gio.bus_unwatch_name(watch_id)
    return t


class TestDBus(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        for k in apt_pkg.config.keys():
            apt_pkg.config.clear(k)
        apt_pkg.init()
        clear_apt_config()
        # create sources.list file
        create_sources_list()
        cls.thread = start_software_properties_dbus_in_session_bus()

    @classmethod
    def tearDownClass(cls):
        global should_quit
        should_quit = True
        cls.thread.join()

    def setUp(self):
        # keep track of signal emissions
        self.sources_list_count = 0
        self.distro_release = get_distro_release()
        self.sources_list_path = create_sources_list()
        # create the client proxy
        bus = dbus.SessionBus(private=True, mainloop=DBusGMainLoop())
        proxy = bus.get_object("com.ubuntu.SoftwareProperties", "/")
        iface = dbus.Interface(proxy, "com.ubuntu.SoftwareProperties")
        self.iface = iface
        self._signal_id = iface.connect_to_signal(
            "SourcesListModified", self._on_sources_list_modified)

    def tearDown(self):
        # ensure we remove the "modified" signal again
        self._signal_id.remove()

    def _on_sources_list_modified(self):
        #print("_on_modified_sources_list")
        self.sources_list_count += 1

    def _debug_sourceslist(self, text=""):
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            logging.debug("sourceslist: %s '%s'" % (text, sourceslist))

    # this is an async call - give it a few seconds to catch up with what we expect
    def _assert_eventually(self, prop, n):
        for i in range(9):
            if getattr(self, prop) == n:
                self.assertEqual(getattr(self, prop), n)
            else:
                time.sleep(1)
        # nope, you die now
        self.assertEqual(getattr(self, prop), n)

    def test_enable_disable_component(self):
        # ensure its not there
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse("universe" in sourceslist)
        # enable
        self.iface.EnableComponent("universe")
        self._debug_sourceslist("2")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertTrue("universe" in sourceslist)
        # disable again
        self.iface.DisableComponent("universe")
        self._debug_sourceslist("3")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse("universe" in sourceslist)
        self._assert_eventually("sources_list_count", 2)

    def test_enable_enable_disable_source_code_sources(self):
        # ensure its not there
        self._debug_sourceslist("4")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse("deb-src" in sourceslist)
        # enable
        self.iface.EnableSourceCodeSources()
        self._debug_sourceslist("5")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertTrue("deb-src" in sourceslist)
        # disable again
        self.iface.DisableSourceCodeSources()
        self._debug_sourceslist("6")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse("deb-src" in sourceslist)
        self._assert_eventually("sources_list_count", 3)

    def test_enable_child_source(self):
        child_source = "%s-updates" % self.distro_release
        # ensure its not there
        self._debug_sourceslist("7")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse(child_source in sourceslist)
        # enable
        self.iface.EnableChildSource(child_source)
        self._debug_sourceslist("8")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertTrue(child_source in sourceslist)
        # disable again
        self.iface.DisableChildSource(child_source)
        self._debug_sourceslist("9")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse(child_source in sourceslist)
        self._assert_eventually("sources_list_count", 2)

    def test_toggle_source(self):
        # test toggle
        source = get_test_source_line()
        self.iface.ToggleSourceUse(source)
        self._debug_sourceslist("10")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
        self.assertTrue("# deb http://archive.ubuntu.com/ubuntu" in sourceslist)
        # to disable the line again, we need to match the new "#"
        source = "# " + source
        self.iface.ToggleSourceUse(source)
        self._debug_sourceslist("11")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse("# deb http://archive.ubuntu.com/ubuntu" in sourceslist)
        self._assert_eventually("sources_list_count", 2)

    def test_replace_entry(self):
        # test toggle
        source = get_test_source_line()
        source_new = "deb http://xxx/ %s" % self.distro_release
        self.iface.ReplaceSourceEntry(source, source_new)
        self._debug_sourceslist("11")
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertTrue(source_new in sourceslist)
            self.assertFalse(source in sourceslist)
        self._assert_eventually("sources_list_count", 1)
        self.iface.ReplaceSourceEntry(source_new, source)
        self._assert_eventually("sources_list_count", 2)

    def test_popcon(self):
        # ensure its set to no
        popcon_p = os.path.join(os.path.dirname(__file__),
                              "aptroot", "etc", "popularity-contest.conf")
        with open(popcon_p) as f:
            popcon = f.read()
            self.assertTrue('PARTICIPATE="no"' in popcon)
        # toggle
        self.iface.SetPopconPariticipation(True)
        with open(popcon_p) as f:
            popcon = f.read()
            self.assertTrue('PARTICIPATE="yes"' in popcon)
            self.assertFalse('PARTICIPATE="no"' in popcon)
        # and back
        self.iface.SetPopconPariticipation(False)
        with open(popcon_p) as f:
            popcon = f.read()
            self.assertFalse('PARTICIPATE="yes"' in popcon)
            self.assertTrue('PARTICIPATE="no"' in popcon)

    def test_updates_automation(self):
        states = [UPDATE_INST_SEC, UPDATE_DOWNLOAD, UPDATE_NOTIFY]
        # security
        self.iface.SetUpdateAutomationLevel(states[0])
        cfg = os.path.join(os.path.dirname(__file__),
                           "aptroot", "etc", "apt", "apt.conf.d",
                           "10periodic")
        with open(cfg) as f:
            config = f.read()
            self.assertTrue('APT::Periodic::Unattended-Upgrade "1";' in config)
        # download
        self.iface.SetUpdateAutomationLevel(states[1])
        with open(cfg) as f:
            config = f.read()
            self.assertTrue('APT::Periodic::Unattended-Upgrade "0";' in config)
            self.assertTrue('APT::Periodic::Download-Upgradeable-Packages "1";' in config)
        # notify
        self.iface.SetUpdateAutomationLevel(states[2])
        with open(cfg) as f:
            config = f.read()
            self.assertTrue('APT::Periodic::Unattended-Upgrade "0";' in config)
            self.assertTrue('APT::Periodic::Download-Upgradeable-Packages "0";' in config)

    def test_updates_interval(self):
        # interval
        self.iface.SetUpdateInterval(0)
        cfg = os.path.join(os.path.dirname(__file__),
                           "aptroot", "etc", "apt", "apt.conf.d",
                           "10periodic")
        with open(cfg) as f:
            config = f.read()
            self.assertTrue(
                'APT::Periodic::Update-Package-Lists' not in config or
                'APT::Periodic::Update-Package-Lists "0";' in config)
        self.iface.SetUpdateInterval(1)
        with open(cfg) as f:
            config = f.read()
            self.assertTrue('APT::Periodic::Update-Package-Lists "1";' in config)
        self.iface.SetUpdateInterval(0)
        with open(cfg) as f:
            config = f.read()
            self.assertTrue('APT::Periodic::Update-Package-Lists "0";' in config)

    def test_add_remove_source_by_line(self):
        # add invalid
        res = self.iface.AddSourceFromLine("xxx")
        self.assertFalse(res)
        # add real
        s = "deb http//ppa.launchpad.net/ foo bar"
        self.iface.AddSourceFromLine(s)
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertTrue(s in sourceslist)
            self.assertTrue(s.replace("deb", "# deb-src") in sourceslist)
        # remove again
        self.iface.RemoveSource(s)
        self.iface.RemoveSource(s.replace("deb", "deb-src"))
        with open(self.sources_list_path) as f:
            sourceslist = f.read()
            self.assertFalse(s in sourceslist)
            self.assertFalse(s.replace("deb", "# deb-src") in sourceslist)
        self._assert_eventually("sources_list_count", 4)

    def test_add_gpg_key(self):
        # clean
        gpg_glob = os.path.join(os.path.dirname(__file__),
                              "aptroot", "etc", "apt", "*.gpg")
        trusted_gpg = os.path.join(os.path.dirname(__file__),
                                   "aptroot", "etc", "apt", "trusted.gpg")
        testkey = os.path.join(os.path.dirname(__file__), 
                               "data", "testkey.gpg")
        for f in glob.glob(gpg_glob):
            os.remove(f)
        self.assertTrue(len(glob.glob(gpg_glob)) == 0)
        # add key from file
        res = self.iface.AddKey(os.path.join(os.path.dirname(__file__),
                                             "data", "testkey.gpg"))
        self.assertTrue(res)
        self.assertEqual(len(glob.glob(gpg_glob)), 1)
        self.assertNotEqual(os.path.getsize(trusted_gpg), 0)
        # remove the key 
        res = self.iface.RemoveKey("D732CA59")
        self.assertTrue(res)
        if os.path.exists(trusted_gpg):
            self.assertEqual(os.path.getsize(trusted_gpg), 0)
        # add from data
        with open(testkey) as keyfile:
            data = keyfile.read()
            res = self.iface.AddKeyFromData(data)
            self.assertTrue(res)
            self.assertEqual(len(glob.glob(gpg_glob)), 1)
            self.assertNotEqual(os.path.getsize(trusted_gpg), 0)
        # remove the key 
        res = self.iface.RemoveKey("D732CA59")
        self.assertTrue(res)
        if os.path.exists(trusted_gpg):        
            self.assertEqual(os.path.getsize(trusted_gpg), 0)
        # test nonsense
        res = self.iface.AddKeyFromData("nonsens")
        self.assertFalse(res)
        # test apt-key update
        res = self.iface.UpdateKeys()
        self.assertTrue(res)
        

if __name__ == "__main__":
    if "-d" in sys.argv:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
    unittest.main()
