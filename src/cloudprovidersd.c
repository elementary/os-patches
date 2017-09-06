/* cloudproviders.c
 *
 * Copyright (C) 2017 Julius Haertl <jus@bitgrid.net>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <cloudprovidermanager.h>

static GMainLoop *loop;
static CloudProviderManager *manager;
static guint dbus_owner_id;

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (name != NULL && name[0] != '\0');

  g_debug ("Connected to the session bus");
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_return_if_fail (connection == NULL || G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (name != NULL && name[0] != '\0');

  g_info ("Lost (or failed to acquire) the name %s on the session message bus", name);
  g_main_loop_quit (loop);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (name != NULL && name[0] != '\0');

  g_debug ("Acquired the name %s on the session message bus", name);

  manager = cloud_provider_manager_new (connection);
  cloud_provider_manager_export (manager);
}

gint
main (gint   argc, gchar *argv[])
{
  loop = g_main_loop_new(NULL, FALSE);

  dbus_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  CLOUD_PROVIDER_MANAGER_DBUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  on_bus_acquired,
                                  on_name_acquired,
                                  on_name_lost,
                                  NULL,
                                  NULL);
  g_main_loop_run(loop);
  g_object_unref(manager);
  g_free(loop);
  return 0;
}
