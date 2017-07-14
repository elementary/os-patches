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
#include "cloudproviderproxy.h"
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

typedef struct
{
  GList *providers;
  guint dbus_owner_id;
  GDBusNodeInfo *dbus_node_info;
  GVariantBuilder *provider_object_managers;
  GVariant *providers_objects;
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


#define KEY_FILE_GROUP "Cloud Provider"

/**
 * load_cloud_provider
 * @manager: A CloudProviders
 * @file: A GFile
 */
static void
load_cloud_provider (CloudProviderManager *self,
                     GFile                *file,
                     GVariantBuilder *builder)
{
  GKeyFile *key_file;
  gchar *path;
  GError *error = NULL;
  gchar *bus_name;
  gchar *object_path;
  gboolean success = FALSE;

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

  g_variant_builder_add(builder, "(so)", bus_name, object_path);

  success = TRUE;
out:
  if (!success)
    g_warning ("Error while loading cloud provider key file at %s", path);
  g_key_file_free (key_file);
}


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

  priv->provider_object_managers = g_variant_builder_new(G_VARIANT_TYPE("a(so)"));

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
           load_cloud_provider (manager, g_file_enumerator_get_child (file_enumerator, info), priv->provider_object_managers);
           info = g_file_enumerator_next_file (file_enumerator, NULL, &error);
        }
    }

    g_free(priv->providers_objects);
    priv->providers_objects = g_variant_builder_end(priv->provider_object_managers);
    g_variant_builder_unref (priv->provider_object_managers);
    g_print("%s\n", g_variant_print(priv->providers_objects, TRUE));

}


void
handle_get_cloud_providers (CloudProviderManager1 *interface,
                            GDBusMethodInvocation  *invocation,
                            gpointer                user_data)
{
  CloudProviderManagerPrivate *priv = cloud_provider_manager_get_instance_private (CLOUD_PROVIDER_MANAGER(user_data));
  g_variant_ref(priv->providers_objects);
  g_print("=> %s\n", g_variant_print(priv->providers_objects, TRUE));
  cloud_provider_manager1_complete_get_cloud_providers (interface, invocation, priv->providers_objects);
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
  g_signal_connect (priv->skeleton,
		    "handle-get-cloud-providers",
		    G_CALLBACK(handle_get_cloud_providers),
		    CLOUD_PROVIDER_MANAGER(self));
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
      cloud_provider_manager_update (CLOUD_PROVIDER_MANAGER(self));
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
