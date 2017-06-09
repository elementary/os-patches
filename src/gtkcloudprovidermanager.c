/* gtkcloudprovidermanager.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
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

#include "gtkcloudprovidermanager.h"
#include "gtkcloudprovider.h"
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#define KEY_FILE_GROUP "Gtk Cloud Provider"

typedef struct
{
  GList *providers;
  guint dbus_owner_id;
  GDBusNodeInfo *dbus_node_info;
} GtkCloudProviderManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkCloudProviderManager, gtk_cloud_provider_manager, G_TYPE_OBJECT)

enum
{
  CHANGED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static const gchar manager_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.CloudProviderManager1'>"
  "    <method name='CloudProviderChanged'>"
  "    </method>"
  "  </interface>"
  "</node>";

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  if (g_strcmp0 (method_name, "CloudProviderChanged") == 0)
    {
      gtk_cloud_provider_manager_update (GTK_CLOUD_PROVIDER_MANAGER (user_data));
    }
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GtkCloudProviderManager *self = user_data;
  GtkCloudProviderManagerPrivate *priv = gtk_cloud_provider_manager_get_instance_private (self);
  guint registration_id;

  g_debug ("Registering cloud provider server 'MyCloud'\n");
  registration_id = g_dbus_connection_register_object (connection,
                                                       GTK_CLOUD_PROVIDER_MANAGER_DBUS_PATH,
                                                       priv->dbus_node_info->interfaces[0],
                                                       &interface_vtable,
                                                       self,
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);

  /* In case some provider updated before adquiring the bus */
  gtk_cloud_provider_manager_update (GTK_CLOUD_PROVIDER_MANAGER (user_data));
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
}

/**
 * gtk_cloud_provider_manager_dup_singleton
 * Returns: (transfer none): A manager singleton
 */
GtkCloudProviderManager *
gtk_cloud_provider_manager_dup_singleton (void)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      GtkCloudProviderManagerPrivate *priv;

      self = g_object_new (GTK_TYPE_CLOUD_PROVIDER_MANAGER, NULL);
      priv = gtk_cloud_provider_manager_get_instance_private (GTK_CLOUD_PROVIDER_MANAGER (self));

      /* Export the interface we listen to, so clients can request properties of
       * the cloud provider such as name, status or icon */
      priv->dbus_node_info = g_dbus_node_info_new_for_xml (manager_xml, NULL);
      g_assert (priv->dbus_node_info != NULL);

      priv->dbus_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                            GTK_CLOUD_PROVIDER_MANAGER_DBUS_NAME,
                                            G_BUS_NAME_OWNER_FLAGS_NONE,
                                            on_bus_acquired,
                                            on_name_acquired,
                                            on_name_lost,
                                            self,
                                            NULL);
      return GTK_CLOUD_PROVIDER_MANAGER (self);
    }
  else
    {
      return g_object_ref (self);
    }
}

static void
gtk_cloud_provider_manager_finalize (GObject *object)
{
  GtkCloudProviderManager *self = (GtkCloudProviderManager *)object;
  GtkCloudProviderManagerPrivate *priv = gtk_cloud_provider_manager_get_instance_private (self);

  g_list_free_full (priv->providers, g_object_unref);
  g_bus_unown_name (priv->dbus_owner_id);
  g_dbus_node_info_unref (priv->dbus_node_info);

  G_OBJECT_CLASS (gtk_cloud_provider_manager_parent_class)->finalize (object);
}

static void
gtk_cloud_provider_manager_class_init (GtkCloudProviderManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_cloud_provider_manager_finalize;

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
}

static void
gtk_cloud_provider_manager_init (GtkCloudProviderManager *self)
{
}

/**
 * gtk_cloud_provider_manager_get_providers
 * @manager: A GtkCloudProviderManager
 * Returns: (transfer none): The list of providers.
 */
GList*
gtk_cloud_provider_manager_get_providers (GtkCloudProviderManager *manager)
{
  GtkCloudProviderManagerPrivate *priv = gtk_cloud_provider_manager_get_instance_private (manager);

  return priv->providers;
}

static void
on_cloud_provider_changed (GtkCloudProvider        *cloud_provider,
                           GtkCloudProviderManager *self)
{
  GIcon *icon;
  gchar *name;
  guint status;

  name = gtk_cloud_provider_get_name (cloud_provider);
  icon = gtk_cloud_provider_get_icon (cloud_provider);
  status = gtk_cloud_provider_get_status (cloud_provider);
  if (name == NULL || icon == NULL || status == GTK_CLOUD_PROVIDER_STATUS_INVALID)
    return;

  g_signal_emit_by_name (self, "changed", NULL);
}

static void
load_cloud_provider (GtkCloudProviderManager *self,
                     GFile                   *file)
{
  GtkCloudProviderManagerPrivate *priv = gtk_cloud_provider_manager_get_instance_private (self);
  GKeyFile *key_file;
  gchar *path;
  GError *error = NULL;
  gchar *bus_name;
  gchar *object_path;
  gboolean success = FALSE;
  GtkCloudProvider *cloud_provider;

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
  cloud_provider = gtk_cloud_provider_new (bus_name, object_path);
  g_signal_connect (cloud_provider, "changed",
                    G_CALLBACK (on_cloud_provider_changed), self);
  priv->providers = g_list_append (priv->providers, cloud_provider);

  success = TRUE;
out:
  if (!success)
    g_warning ("Error while loading cloud provider key file at %s", path);
  g_key_file_free (key_file);
}

/**
 * gtk_cloud_provider_manager_update
 * @manager: A GtkCloudProviderManager
 */
void
gtk_cloud_provider_manager_update (GtkCloudProviderManager *manager)
{
  GtkCloudProviderManagerPrivate *priv = gtk_cloud_provider_manager_get_instance_private (manager);
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
