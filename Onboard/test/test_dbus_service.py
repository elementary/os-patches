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

import unittest
import subprocess
from contextlib import contextmanager

import dbus
from dbus.mainloop.glib import DBusGMainLoop

from gi.repository import GLib


class TestDBusService(unittest.TestCase):

    def test_service(self):
        DBUS_NAME  = "org.onboard.Onboard"
        DBUS_PATH  = "/org/onboard/Onboard/Keyboard"
        DBUS_IFACE = "org.onboard.Onboard.Keyboard"

        bus = dbus.SessionBus(mainloop=DBusGMainLoop())

        try:
            process = subprocess.Popen(["./onboard"])
            self.wait_name_owner_changed(bus, DBUS_NAME)
            self.assertTrue(bus.name_has_owner(DBUS_NAME))

            proxy = bus.get_object(DBUS_NAME, DBUS_PATH)
            keyboard = dbus.Interface(proxy, DBUS_IFACE)
            keyboard_props = dbus.Interface(proxy, dbus.PROPERTIES_IFACE)

            keyboard.Show()
            self.assertEqual(keyboard_props.Get(DBUS_IFACE, "Visible"), True)

            with self.wait_property_changed(keyboard_props, "Visible"):
                keyboard.Hide()
            self.assertEqual(keyboard_props.Get(DBUS_IFACE, "Visible"), False)

            with self.wait_property_changed(keyboard_props, "Visible"):
                keyboard.Show()
            self.assertEqual(keyboard_props.Get(DBUS_IFACE, "Visible"), True)

            with self.wait_property_changed(keyboard_props, "Visible"):
                keyboard.ToggleVisible()
            self.assertEqual(keyboard_props.Get(DBUS_IFACE, "Visible"), False)

            with self.wait_property_changed(keyboard_props, "Visible"):
                keyboard.ToggleVisible()
            self.assertEqual(keyboard_props.Get(DBUS_IFACE, "Visible"), True)
        finally:
            process.terminate()

    @staticmethod
    def wait_name_owner_changed(bus, name):

        def on_name_owner_changed(name, old, new):
            if not old:
                loop.quit()

        bus.add_signal_receiver(on_name_owner_changed,
                                "NameOwnerChanged",
                                dbus.BUS_DAEMON_IFACE,
                                arg0 = name)
        loop = GLib.MainLoop()
        loop.run()

    @staticmethod
    @contextmanager
    def wait_property_changed(iface, property):

        def on_signal(iface, changed, invalidated):
            if property in changed:
                loop.quit()

        iface.connect_to_signal("PropertiesChanged", on_signal)

        yield None

        loop = GLib.MainLoop()
        loop.run()


if __name__ == "__main__":
    unittest.main()
