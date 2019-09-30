/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include "glib-fixture.h"

/***
****
***/

class TestDBusFixture: public GlibFixture
{
  public:

    TestDBusFixture() {}

    TestDBusFixture(const std::vector<std::string>& service_dirs_in): service_dirs(service_dirs_in) {}

  private:

    typedef GlibFixture super;

    static void
    on_bus_opened (GObject* /*object*/, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<TestDBusFixture*>(gself);

      GError * err = 0;
      self->system_bus = g_bus_get_finish (res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

    static void
    on_bus_closed (GObject* /*object*/, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<TestDBusFixture*>(gself);

      GError * err = 0;
      g_dbus_connection_close_finish (self->system_bus, res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

  protected:

    GTestDBus * test_dbus;
    GDBusConnection * system_bus;
    const std::vector<std::string> service_dirs;

    virtual void SetUp ()
    {
      super::SetUp ();

      // pull up a test dbus
      test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
      for (const auto& dir : service_dirs)
        g_test_dbus_add_service_dir (test_dbus, dir.c_str());
      g_test_dbus_up (test_dbus);
      const char * address = g_test_dbus_get_bus_address (test_dbus);
      g_setenv ("DBUS_SYSTEM_BUS_ADDRESS", address, true);
      g_setenv ("DBUS_SESSION_BUS_ADDRESS", address, true);
      g_debug ("test_dbus's address is %s", address);

      // wait for the GDBusConnection before returning
      g_bus_get (G_BUS_TYPE_SYSTEM, nullptr, on_bus_opened, this);
      g_main_loop_run (loop);
    }

    virtual void TearDown ()
    {
      wait_msec();

      // close the system bus
      g_dbus_connection_close(system_bus, nullptr, on_bus_closed, this);
      g_main_loop_run(loop);
      g_clear_object(&system_bus);

      // tear down the test dbus
      g_test_dbus_down(test_dbus);
      g_clear_object(&test_dbus);

      super::TearDown();
    }
};
