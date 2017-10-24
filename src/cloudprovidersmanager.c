/* cloudprovidersmanager.c
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

#include "cloudprovidersmanager.h"
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

struct _CloudProvidersManager
{
  GObject parent;

  GList *providers;
  GDBusConnection *connection;
  GDBusNodeInfo *dbus_node_info;
  GVariant *providers_objects;
  CloudProvidersDbusManager *skeleton;
};

G_DEFINE_TYPE (CloudProvidersManager, cloud_providers_manager, G_TYPE_OBJECT)

#define KEY_FILE_GROUP "Cloud Providers"

/**
 * load_cloud_provider
 * @manager: A CloudProvidersManager
 * @file: A GFile
 */
static void
load_cloud_provider (CloudProvidersManager *self,
                     GFile                 *file,
                     GVariantBuilder       *builder)
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
  {
    g_warning ("Error while loading cloud provider key file at %s", path);
    goto out;
  }

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
  g_free (bus_name);
  g_free (object_path);
out:
  if (!success)
    g_warning ("Error while loading cloud provider key file at %s", path);
  g_key_file_free (key_file);
  g_object_unref (file);
  g_free (path);
}

static void
cloud_providers_manager_update (CloudProvidersManager *self)
{
  const gchar* const *data_dirs;
  gint i;
  gint len;
  gchar *key_files_directory_path;
  GFile *key_files_directory_file;
  GError *error = NULL;
  GFileEnumerator *file_enumerator;
  GVariantBuilder *providers_dbus_objects;

  providers_dbus_objects = g_variant_builder_new(G_VARIANT_TYPE("a(so)"));

  g_list_free_full (self->providers, g_object_unref);
  self->providers = NULL;

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
           load_cloud_provider (self, g_file_enumerator_get_child (file_enumerator, info), providers_dbus_objects);
           g_object_unref (info);
           info = g_file_enumerator_next_file (file_enumerator, NULL, &error);
        }
      g_object_unref (file_enumerator);
      g_free (key_files_directory_path);
      g_object_unref (key_files_directory_file);
    }

    g_free(self->providers_objects);
    self->providers_objects = g_variant_builder_end(providers_dbus_objects);
    g_variant_builder_unref (providers_dbus_objects);

    gchar *provider_debug = g_variant_print(self->providers_objects, TRUE);
    g_debug("Manager loading provider: %s\n", provider_debug);
    g_free(provider_debug);
}

CloudProvidersManager *
cloud_providers_manager_new (GDBusConnection *connection)
{
  CloudProvidersManager *self;

  self = g_object_new (CLOUD_PROVIDERS_TYPE_MANAGER, NULL);

  self->connection = connection;
  self->providers = NULL;
  self->skeleton = cloud_providers_dbus_manager_skeleton_new ();
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                    self->connection,
                                    CLOUD_PROVIDERS_MANAGER_DBUS_PATH,
                                    NULL);
  cloud_providers_manager_update (CLOUD_PROVIDERS_MANAGER(self));
  cloud_providers_dbus_manager_set_providers (self->skeleton, self->providers_objects);

  return self;
}

static void
cloud_providers_manager_finalize (GObject *object)
{
  CloudProvidersManager *self = (CloudProvidersManager *)object;

  g_list_free_full (self->providers, g_object_unref);
  g_dbus_node_info_unref (self->dbus_node_info);

  G_OBJECT_CLASS (cloud_providers_manager_parent_class)->finalize (object);
}

static void
cloud_providers_manager_class_init (CloudProvidersManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_providers_manager_finalize;
}

static void
cloud_providers_manager_init (CloudProvidersManager *self)
{
}
