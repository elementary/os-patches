#!/usr/bin/env python3

import unittest
import dbus
from dbus.mainloop.glib import DBusGMainLoop
import dbusmock
import subprocess
from gi.repository import GLib, Gio, MessagingMenu

DBusGMainLoop(set_as_default=True)

class MessagingMenuTest(dbusmock.DBusTestCase):
    @classmethod
    def setUpClass(klass):
        klass.start_session_bus()
        klass.bus = klass.get_dbus(False)

    def setUp(self):
        name = 'com.canonical.indicator.messages'
        obj_path = '/com/canonical/indicator/messages/service'
        iface = 'com.canonical.indicator.messages.service'

        self.messaging_service = self.spawn_server(name, obj_path, iface, stdout=subprocess.PIPE)
        self.mock = dbus.Interface(self.bus.get_object(name, obj_path), dbusmock.MOCK_IFACE)
        self.mock.AddMethod('', 'RegisterApplication', 'so', '', '')
        self.mock.AddMethod('', 'UnregisterApplication', 's', '', '')
        self.mock.AddMethod('', 'ApplicationStoppedRunning', 's', '', '')
        self.mock.AddMethod('', 'SetStatus', 'ss', '', '')

        self.loop = GLib.MainLoop()

    def tearDown(self):
        self.messaging_service.terminate()
        self.messaging_service.wait()

    def assertArgumentsEqual(self, args, *expected_args):
        self.assertEqual(len(args), len(expected_args))
        for i in range(len(args)):
            if expected_args[i]:
                self.assertEqual(args[i], expected_args[i])

    def assertMethodCalled(self, name, *expected_args):
        # set a flag on timeout, assertions don't get bubbled up through c functions
        self.timed_out = False
        def timeout(): self.timed_out = True
        timeout_id = GLib.timeout_add_seconds(10, timeout)
        while 1:
            calls = self.mock.GetMethodCalls(name)
            if len(calls) > 0:
                GLib.source_remove(timeout_id)
                self.assertArgumentsEqual(calls[0][1], *expected_args)
                break
            GLib.MainContext.default().iteration(True)
            if self.timed_out:
                raise self.failureException('method %s was not called after 10 seconds' % name)

    def test_registration(self):
        mmapp = MessagingMenu.App.new('test.desktop')
        mmapp.register()
        self.assertMethodCalled('RegisterApplication', 'test.desktop', None)

        mmapp.unregister()
        self.assertMethodCalled('UnregisterApplication', 'test.desktop')

        # ApplicationStoppedRunning is called when the last ref on mmapp is dropped
        # Since mmapp is the only thing holding on to a GDBusConnection, the
        # connection might get freed before it sends the StoppedRunning
        # message. Flush the connection to make sure it is sent.
        bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        bus.flush_sync(None)
        del mmapp
        self.assertMethodCalled('ApplicationStoppedRunning', 'test.desktop')

    def test_status(self):
        mmapp = MessagingMenu.App.new('test.desktop')
        mmapp.register()
        mmapp.set_status(MessagingMenu.Status.AWAY)
        self.assertMethodCalled('SetStatus', 'test.desktop', 'away')

unittest.main(testRunner=unittest.TextTestRunner())
