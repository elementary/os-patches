/* cloudprovider.c
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

#include "cloudprovider.h"
#include "cloudprovider-generated.h"
#include <gio/gio.h>

typedef struct
{
  GDBusConnection *bus;
  GDBusObjectManagerServer *manager;
  gchar *bus_name;
  gchar *object_path;
  GCancellable *cancellable;
  GHashTable *menuModels;
  GHashTable *actionGroups;
} CloudProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProvider, cloud_provider, G_TYPE_OBJECT)

void
cloud_provider_export_account(CloudProvider* cloud_provider,
                              const gchar *account_name,
                              CloudProviderAccount1 *account)
{
  CloudProviderPrivate *priv = cloud_provider_get_instance_private(cloud_provider);
  CloudProviderObjectSkeleton *object;
  gchar *object_path = g_strconcat (priv->object_path, "/", account_name, NULL);
  g_print("%s\n", object_path);
  object = cloud_provider_object_skeleton_new(object_path);
  cloud_provider_object_skeleton_set_account1(object, account);
  g_dbus_object_manager_server_export (priv->manager,
                                       G_DBUS_OBJECT_SKELETON(object));
  g_free(object_path);
}

void
cloud_provider_unexport_account(CloudProvider* cloud_provider,
                                const gchar *account_name)
{
  CloudProviderPrivate *priv = cloud_provider_get_instance_private(cloud_provider);
  gchar *object_path = g_strconcat (priv->object_path, "/", account_name, NULL);
  g_print(object_path);
  g_dbus_object_manager_server_unexport (priv->manager, object_path);
  guint *export_id;
  export_id = (guint*)g_hash_table_lookup(priv->menuModels, account_name);
  if(export_id != NULL) {
    g_dbus_connection_unexport_menu_model(priv->bus, *export_id);
    g_free(export_id);
  }
  export_id = (guint*)g_hash_table_lookup(priv->actionGroups, account_name);
  if(export_id != NULL) {
    g_dbus_connection_unexport_action_group(priv->bus, *export_id);
    g_free(export_id);
  }
  g_free (object_path);
}

guint
cloud_provider_export_menu(CloudProvider* cloud_provider,
                           const gchar *account_name,
                           GMenuModel *model)
{
  CloudProviderPrivate *priv = cloud_provider_get_instance_private(cloud_provider);
  gchar *object_path = g_strconcat(priv->object_path, "/", account_name, NULL);
  GError *error = NULL;
  g_print ("Exporting menus on the bus...\n");
  guint *export_id = g_new0(guint, 1);
  *export_id = g_dbus_connection_export_menu_model (priv->bus, object_path, model, &error);
  if (!*export_id)
    {
      g_warning ("Menu export failed: %s", error->message);
      return 0;
    }
  g_hash_table_insert(priv->menuModels, g_strdup(account_name), export_id);
  return *export_id;
}

guint
cloud_provider_export_actions(CloudProvider* cloud_provider,
                              const gchar *account_name,
                              GActionGroup *action_group)
{
  CloudProviderPrivate *priv = cloud_provider_get_instance_private(cloud_provider);
  gchar *object_path = g_strconcat(priv->object_path, "/", account_name, NULL);
  GError *error = NULL;
  guint *export_id = g_new0(guint, 1);
  *export_id = g_dbus_connection_export_action_group (priv->bus, object_path, action_group, &error);
  g_print ("Exporting actions on the bus...\n");
  if (!*export_id)
    {
      g_warning ("Action export failed: %s", error->message);
      return 0;
    }
  g_hash_table_insert(priv->actionGroups, g_strdup(account_name), export_id);
  return *export_id;
}

void
cloud_provider_export_objects(CloudProvider* cloud_provider)
{
  CloudProviderPrivate *priv = cloud_provider_get_instance_private(cloud_provider);
  g_dbus_object_manager_server_set_connection (priv->manager, priv->bus);
}

void
cloud_provider_emit_changed (CloudProvider *cloud_provider, const gchar *account_name)
{
  CloudProviderPrivate *priv = cloud_provider_get_instance_private(cloud_provider);
  gchar *object_path = g_strconcat(priv->object_path, "/", account_name, NULL);
  GDBusObject *object = g_dbus_object_manager_get_object((GDBusObjectManager*)priv->manager, object_path);
  CloudProviderAccount1 *account = cloud_provider_object_get_account1 (CLOUD_PROVIDER_OBJECT(object));
  cloud_provider_account1_emit_cloud_provider_changed (account);
  g_object_unref (account);
  g_object_unref (object);
  g_free (object_path);
}

CloudProvider*
cloud_provider_new (GDBusConnection *bus,
                    const gchar *bus_name,
                    const gchar *object_path)
{
  CloudProvider *self;
  CloudProviderPrivate *priv;

  self = g_object_new (TYPE_CLOUD_PROVIDER, NULL);
  priv = cloud_provider_get_instance_private (self);

  priv->bus_name = g_strdup (bus_name);
  priv->object_path = g_strdup (object_path);
  priv->bus = bus;
  priv->cancellable = g_cancellable_new ();
  priv->manager = g_dbus_object_manager_server_new (object_path);

  priv->menuModels = g_hash_table_new(g_str_hash, g_str_equal);
  priv->actionGroups = g_hash_table_new(g_str_hash, g_str_equal);
    return self;
}

static void
cloud_provider_finalize (GObject *object)
{
  CloudProvider *self = (CloudProvider *)object;
  CloudProviderPrivate *priv = cloud_provider_get_instance_private (self);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->bus);
  g_free (priv->bus_name);
  g_free (priv->object_path);
  g_object_unref(priv->manager);

  g_hash_table_remove_all(priv->menuModels);
  g_object_unref(priv->menuModels);
  g_hash_table_remove_all(priv->actionGroups);
  g_object_unref(priv->actionGroups);

  G_OBJECT_CLASS (cloud_provider_parent_class)->finalize (object);
}

static void
cloud_provider_class_init (CloudProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = cloud_provider_finalize;
}

static void
cloud_provider_init (CloudProvider *self)
{
}
