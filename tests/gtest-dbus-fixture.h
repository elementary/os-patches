/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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
 */

#include <glib.h>
#include <gio/gio.h>

#include <gtest/gtest.h>

/***
****
***/

class GTestDBusFixture : public ::testing::Test
{
  private:

    static void
    on_bus_opened (GObject * o G_GNUC_UNUSED, GAsyncResult * res, gpointer gself)
    {
      GTestDBusFixture * self = static_cast<GTestDBusFixture*>(gself);

      GError * err = 0;
      self->conn = g_bus_get_finish (res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

    static void
    on_bus_closed (GObject * o G_GNUC_UNUSED, GAsyncResult * res, gpointer gself)
    {
      GTestDBusFixture * self = static_cast<GTestDBusFixture*>(gself);

      GError * err = 0;
      g_dbus_connection_close_finish (self->conn, res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->loop);
    }

  protected:

    virtual void SetUp ()
    {
      conn = 0;
      test_dbus = 0;
      loop = 0;

      g_setenv ("GSETTINGS_SCHEMA_DIR", SCHEMA_DIR, TRUE);
      g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
      g_debug ("SCHEMA_DIR is %s", SCHEMA_DIR);

      // pull up a test dbus
      loop = g_main_loop_new (NULL, FALSE);
      test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
      g_test_dbus_add_service_dir (test_dbus, INDICATOR_SERVICE_DIR);
      g_debug ("INDICATOR_SERVICE_DIR is %s", INDICATOR_SERVICE_DIR);
      g_test_dbus_up (test_dbus);
      const char * address;
      address = g_test_dbus_get_bus_address (test_dbus);
      g_setenv ("DBUS_SYSTEM_BUS_ADDRESS", address, TRUE);
      g_debug ("test_dbus's address is %s", address);

      // wait for the GDBusConnection before returning
      g_bus_get (G_BUS_TYPE_SYSTEM, NULL, on_bus_opened, this);
      g_main_loop_run (loop);
    }

    virtual void TearDown()
    {
      // close the bus connection
      g_dbus_connection_close (conn, NULL, on_bus_closed, this);
      g_main_loop_run (loop);
      g_clear_object (&conn);

      // tear down the test dbus
      g_test_dbus_down (test_dbus);
      g_clear_object (&test_dbus);

      g_clear_pointer (&loop, g_main_loop_unref);
    }

  private:

    static gboolean
    wait_for_signal__timeout(gpointer name)
    {
      g_error("%s: timed out waiting for signal '%s'", G_STRLOC, (char*)name);
      return G_SOURCE_REMOVE;
    }

    static gboolean
    wait_msec__timeout(gpointer loop)
    {
      g_main_loop_quit(static_cast<GMainLoop*>(loop));
      return G_SOURCE_CONTINUE;
    }

  protected:

    /* convenience func to loop while waiting for a GObject's signal */
    void wait_for_signal(gpointer o, const gchar * signal, const int timeout_seconds=5)
    {
      // wait for the signal or for timeout, whichever comes first
      const auto handler_id = g_signal_connect_swapped(o, signal,
                                                       G_CALLBACK(g_main_loop_quit),
                                                       loop);
      const auto timeout_id = g_timeout_add_seconds(timeout_seconds,
                                                    wait_for_signal__timeout,
                                                    loop);
      g_main_loop_run(loop);
      g_source_remove(timeout_id);
      g_signal_handler_disconnect(o, handler_id);
    }

    /* convenience func to loop for N msec */
    void wait_msec(int msec=50)
    {
      const auto id = g_timeout_add(msec, wait_msec__timeout, loop);
      g_main_loop_run(loop);
      g_source_remove(id);
    }

    GMainLoop * loop;
    GTestDBus * test_dbus;
    GDBusConnection * conn;
};
