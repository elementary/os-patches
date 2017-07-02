/* cloudprovidermanager.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
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

#include "cloudprovidermanager.h"
#include "cloudprovider.h"
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#define KEY_FILE_GROUP "Cloud Provider"

typedef struct
{
  GList *providers;
  guint dbus_owner_id;
  GDBusNodeInfo *dbus_node_info;
  GHashTable* provider_object_managers;
  CloudProviderManager1 *skeleton;
} CloudProviderManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProviderManager, cloud_provider_manager, G_TYPE_OBJECT)

enum
{
  CHANGED,
  OWNERS_CHANGED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static void
on_cloud_provider_changed (CloudProvider        *cloud_provider,
                           CloudProviderManager *self)
{
  if(!cloud_provider_is_available(cloud_provider))
    return;
  g_signal_emit_by_name (self, "changed", NULL);
}

static void
on_cloud_provider_changed_notify (CloudProvider *cloud_provider, CloudProviderManager *self)
{
  if(!cloud_provider_is_available(cloud_provider))
    return;

  // update manager to remove cloud providers after owner disappeared
  if(cloud_provider_get_owner(cloud_provider) == NULL) {
    cloud_provider_manager_update(self);
    g_signal_emit_by_name (self, "changed", NULL);
    g_signal_emit_by_name (self, "owners-changed", NULL);
  }
}

static void
on_cloud_provider_object_manager_notify (
GObject    *object,
                      GParamSpec *pspec,
                      gpointer user_data)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (object);
  CloudProviderManager *self = CLOUD_PROVIDER_MANAGER(user_data);
  gchar *name_owner;

  name_owner = g_dbus_object_manager_client_get_name_owner (manager);
  g_print ("name-owner: %s\n", name_owner);
  cloud_provider_manager_update(self);
  g_free (name_owner);
  g_signal_emit_by_name (self, "changed", NULL);
}


static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  CloudProviderManager *self = user_data;
  CloudProviderManagerPrivate *priv = cloud_provider_manager_get_instance_private (CLOUD_PROVIDER_MANAGER (user_data));
  g_signal_connect_swapped (priv->skeleton,
		    "handle-cloud-provider-changed",
		    G_CALLBACK(cloud_provider_manager_update),
		    self);
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (priv->skeleton),
				    connection,
				    CLOUD_PROVIDER_MANAGER_DBUS_PATH,
				    NULL);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  CloudProviderManagerPrivate *priv = cloud_provider_manager_get_instance_private (CLOUD_PROVIDER_MANAGER (user_data));
  if (g_dbus_interface_skeleton_has_connection (G_DBUS_INTERFACE_SKELETON (priv->skeleton), connection))
    {
      g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(priv->skeleton));
    }
}

/**
 * cloud_provider_manager_dup_singleton
 * Returns: (transfer none): A manager singleton
 */
CloudProviderManager *
cloud_provider_manager_dup_singleton (void)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      CloudProviderManagerPrivate *priv;
      self = g_object_new (TYPE_CLOUD_PROVIDER_MANAGER, NULL);
      priv = cloud_provider_manager_get_instance_private (CLOUD_PROVIDER_MANAGER(self));
      priv->dbus_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                            CLOUD_PROVIDER_MANAGER_DBUS_NAME,
                                            G_BUS_NAME_OWNER_FLAGS_NONE,
                                            on_bus_acquired,
                                            on_name_acquired,
                                            on_name_lost,
                                            self,
                                            NULL);
      priv->skeleton = cloud_provider_manager1_skeleton_new ();
      priv->provider_object_managers = g_hash_table_new(g_str_hash, g_str_equal);
      return CLOUD_PROVIDER_MANAGER (self);
    }
  else
    {
      return g_object_ref (self);
    }
}

static void
cloud_provider_manager_finalize (GObject *object)
{
  CloudProviderManager *self = (CloudProviderManager *)object;
  CloudProviderManagerPrivate *priv = cloud_provider_manager_get_instance_private (self);

  g_list_free_full (priv->providers, g_object_unref);
  g_bus_unown_name (priv->dbus_owner_id);
  g_dbus_node_info_unref (priv->dbus_node_info);

  G_OBJECT_CLASS (cloud_provider_manager_parent_class)->finalize (object);
}

static void
cloud_provider_manager_class_init (CloudProviderManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_provider_manager_finalize;

  gSignals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);
 gSignals [OWNERS_CHANGED] =
    g_signal_new ("owners-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);
}

static void
cloud_provider_manager_init (CloudProviderManager *self)
{
}

/**
 * cloud_provider_manager_get_providers
 * @manager: A CloudProviderManager
 * Returns: (transfer none): The list of providers.
 */
GList*
cloud_provider_manager_get_providers (CloudProviderManager *manager)
{
  CloudProviderManagerPrivate *priv = cloud_provider_manager_get_instance_private (manager);

  return priv->providers;
}

static void
load_cloud_provider (CloudProviderManager *self,
                     GFile                   *file)
{
  CloudProviderManagerPrivate *priv = cloud_provider_manager_get_instance_private (self);
  GKeyFile *key_file;
  gchar *path;
  GError *error = NULL;
  gchar *bus_name;
  gchar *object_path;
  gboolean success = FALSE;
  CloudProvider *cloud_provider;

  key_file = g_key_file_new ();
  path = g_file_get_path (file);
  g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, &error);
  if (error != NULL)
    goto out;

  if (!g_key_file_has_group (key_file, KEY_FILE_GROUP))
    goto out;

  bus_name = g_key_file_get_string (key_file, KEY_FILE_GROUP, "BusName", &error);
  if (error != NULL)
    goto out;
  object_path = g_key_file_get_string (key_file, KEY_FILE_GROUP, "ObjectPath", &error);
  if (error != NULL)
    goto out;

  g_print ("cloud provider found %s %s\n", bus_name, object_path);
  GDBusObjectManager *manager = g_hash_table_lookup(priv->provider_object_managers, bus_name);
  if(manager == NULL) {
    manager = object_manager_client_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                     G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                     bus_name,
                                                     object_path,
                                                     NULL,
                                                     &error);

    if (manager == NULL)
      {
        g_printerr ("Error getting object manager client: %s", error->message);
        g_error_free (error);
        goto out;
      }

    g_signal_connect(manager, "notify::name-owner", G_CALLBACK(on_cloud_provider_object_manager_notify), self);
    g_hash_table_insert(priv->provider_object_managers, bus_name, manager);
  }
  GList *objects;
  GList *l;

  g_print ("Object manager at %s\n", g_dbus_object_manager_get_object_path (manager));
  objects = g_dbus_object_manager_get_objects (manager);
  for (l = objects; l != NULL; l = l->next)
    {
      Object *object = OBJECT(l->data);
      g_print (" - Object at %s\n", g_dbus_object_get_object_path (G_DBUS_OBJECT (object)));
      cloud_provider = cloud_provider_new (bus_name, g_dbus_object_get_object_path (G_DBUS_OBJECT (object)));
      g_signal_connect (cloud_provider, "changed",
                    G_CALLBACK (on_cloud_provider_changed), self);
      g_signal_connect (cloud_provider, "changed-notify",
                    G_CALLBACK (on_cloud_provider_changed_notify), self);
      priv->providers = g_list_append (priv->providers, cloud_provider);
    }
  success = TRUE;
out:
  if (!success)
    g_warning ("Error while loading cloud provider key file at %s", path);
  g_key_file_free (key_file);
}

/**
 * cloud_provider_manager_update
 * @manager: A CloudProviderManager
 */
void
cloud_provider_manager_update (CloudProviderManager *manager)
{
  CloudProviderManagerPrivate *priv = cloud_provider_manager_get_instance_private (manager);
  const gchar* const *data_dirs;
  gint i;
  gint len;
  gchar *key_files_directory_path;
  GFile *key_files_directory_file;
  GError *error = NULL;
  GFileEnumerator *file_enumerator;


  g_list_free_full (priv->providers, g_object_unref);
  priv->providers = NULL;

  data_dirs = g_get_system_data_dirs ();
  len = g_strv_length ((gchar **)data_dirs);

  for (i = 0; i < len; i++)
    {
      GFileInfo *info;

      key_files_directory_path = g_build_filename (data_dirs[i], "cloud-providers", NULL);
      key_files_directory_file = g_file_new_for_path (key_files_directory_path);
      file_enumerator = g_file_enumerate_children (key_files_directory_file,
                                                   "standard::name,standard::type",
                                                   G_FILE_QUERY_INFO_NONE,
                                                   NULL,
                                                   &error);
      if (error)
        {
          error = NULL;
          continue;
        }

      info = g_file_enumerator_next_file (file_enumerator, NULL, &error);
      if (error)
        {
          g_warning ("Error while enumerating file %s error: %s\n", key_files_directory_path, error->message);
          error = NULL;
          continue;
        }
      while (info != NULL && error == NULL)
        {
           load_cloud_provider (manager, g_file_enumerator_get_child (file_enumerator, info));
           info = g_file_enumerator_next_file (file_enumerator, NULL, &error);
        }
    }

}
