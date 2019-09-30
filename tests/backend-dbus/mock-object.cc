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

#include "mock-object.h"

namespace
{
  const int TIMEOUT_SECONDS = 5;

  gboolean on_timeout_reached (gpointer loop)
  {
    g_main_loop_quit (static_cast<GMainLoop*>(loop));
    return G_SOURCE_REMOVE;
  }

  void on_name_acquired (GDBusConnection * connection G_GNUC_UNUSED,
                         const char      * name       G_GNUC_UNUSED,
                         gpointer          loop)
  {
    //g_debug ("name '%s' acquired", name);
    g_main_loop_quit (static_cast<GMainLoop*>(loop));
  }

  void on_name_lost (GDBusConnection * connection   G_GNUC_UNUSED,
                     const char      * name         G_GNUC_UNUSED,
                     gpointer          loop)
  {
    //g_debug ("name '%s' lost", name);
    g_main_loop_quit (static_cast<GMainLoop*>(loop));
  }
}

void
MockObject :: set_skeleton (GDBusInterfaceSkeleton * skeleton)
{
  g_assert (skeleton != NULL);
  g_assert (my_skeleton == NULL);
  g_assert (g_variant_is_object_path (my_object_path.c_str()));
  g_assert (my_owner_id == 0);

  my_skeleton = G_DBUS_INTERFACE_SKELETON (g_object_ref (skeleton));

  GError * err = NULL;
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON(my_skeleton),
                                    my_bus_connection,
                                    my_object_path.c_str(),
                                    &err);
  g_assert_no_error (err);

  my_owner_id = g_bus_own_name_on_connection (my_bus_connection,
                                              my_object_name.c_str(),
                                              G_BUS_NAME_OWNER_FLAGS_NONE,
                                              on_name_acquired,
                                              on_name_lost,
                                              my_loop,
                                              NULL);

  // wait for the name to be acquired or timeout, whichever comes first
  const guint timeout_id = g_timeout_add_seconds (TIMEOUT_SECONDS,
                                                  on_timeout_reached,
                                                  my_loop);
  g_main_loop_run (my_loop);
  g_assert (g_main_context_find_source_by_id (NULL, timeout_id) != NULL);
  g_source_remove (timeout_id);
}

/***
****
***/

MockObject :: MockObject (GMainLoop          * loop,
                          GDBusConnection    * bus_connection,
                          const std::string  & object_name,
                          const std::string  & object_path):
  my_owner_id (0),
  my_loop (g_main_loop_ref (loop)),
  my_bus_connection (G_DBUS_CONNECTION (g_object_ref (bus_connection))),
  my_object_name (object_name),
  my_object_path (object_path),
  my_skeleton (0)
{
}

MockObject :: ~MockObject ()
{
  g_main_loop_unref (my_loop);

  if (my_owner_id != 0)
    {
      g_bus_unown_name (my_owner_id);

      my_owner_id = 0;
    }

  if (my_skeleton)
    {
      g_dbus_interface_skeleton_unexport (my_skeleton);

      g_clear_object (&my_skeleton);
    }

  g_clear_object (&my_bus_connection);
}
