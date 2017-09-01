/* cloudproviderexporter.c
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

#include "cloudproviderexporter.h"
#include "cloudprovideraccountexporterpriv.h"
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
} CloudProviderExporterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProviderExporter, cloud_provider_exporter, G_TYPE_OBJECT)

/**
 * SECTION:cloudproviderexporter
 * @title: CloudProviderExporter
 * @short_description: Base object for representing a single provider
 * @include: src/cloudproviderexporter.h
 *
 * #CloudProviderExporter is the basic object that interacts with UI and actions that a
 * provider will present to the user.
 * list view. Extensions can provide #NautilusColumn by registering a
 * #NautilusColumnProvider and returning them from
 * nautilus_column_provider_get_columns(), which will be called by the main
 * application when creating a view.
 */

/**
 * cloud_provider_exporter_add_account:
 * @self: The cloud provider
 * @account: The account object
 *
 * Each cloud provider can have a variety of account associated with it. Use this
 * function to export the accounts the user set up.
 */
void
cloud_provider_exporter_add_account (CloudProviderExporter        *cloud_provider,
                                     CloudProviderAccountExporter *account)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(cloud_provider);
  CloudProviderObjectSkeleton *object;
  gchar *object_path = g_strconcat (priv->object_path, "/", cloud_provider_account_exporter_get_object_name (account), NULL);
  object = cloud_provider_object_skeleton_new(object_path);
  cloud_provider_object_skeleton_set_account1(object, CLOUD_PROVIDER_ACCOUNT1 (cloud_provider_account_exporter_get_skeleton (account)));
  g_dbus_object_manager_server_export (priv->manager, G_DBUS_OBJECT_SKELETON(object));
  g_free(object_path);
}

/**
 * cloud_provider_export_account:
 * @self: The cloud provider
 * @account_name: The account name
 * @account: The account object
 *
 * Each cloud provider can have a variety of account associated with it. Use this
 * function to export the accounts the user set up.
 */
void
cloud_provider_exporter_export_account(CloudProviderExporter * self,
                              const gchar *account_name,
                              CloudProviderAccount1 *account)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  CloudProviderObjectSkeleton *object;
  gchar *object_path = g_strconcat (priv->object_path, "/", account_name, NULL);
  object = cloud_provider_object_skeleton_new(object_path);
  cloud_provider_object_skeleton_set_account1(object, account);
  g_dbus_object_manager_server_export (priv->manager,
                                       G_DBUS_OBJECT_SKELETON(object));
  g_free(object_path);
}

/**
 * cloud_provider_exporter_unexport_account:
 * @self: The cloud provider
 * @account_name: The name of the account
 *
 * Each cloud provider can have a variety of account associated with it. Use this
 * function to remove an already set up account.
 */
void
cloud_provider_exporter_unexport_account(CloudProviderExporter *self,
                                const gchar   *account_name)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  gchar *object_path = g_strconcat (priv->object_path, "/", account_name, NULL);
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

/**
 * cloud_provider_exporter_export_menu:
 * @self: The cloud provider
 * @account_name: The name of the account
 *
 * One of the benefits of the integration is to display a menu with available
 * options for an account. Use this function to export a GMenuModel menu to be
 * displayed by the choosen integration by the desktop environment or application.
 */
guint
cloud_provider_exporter_export_menu(CloudProviderExporter *self,
                           const gchar   *account_name,
                           GMenuModel    *model)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  gchar *object_path = g_strconcat(priv->object_path, "/", account_name, NULL);
  GError *error = NULL;
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

/**
 * cloud_provider_exporter_unexport_menu:
 * @self: The cloud provider
 * @account_name: The name of the account
 *
 * Remove the menu added with cloud_provider_exporter_export_menu
 */
void
cloud_provider_exporter_unexport_menu(CloudProviderExporter *self,
                             const gchar   *account_name)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  guint *export_id;
  export_id = (guint*)g_hash_table_lookup(priv->menuModels, account_name);
  if(export_id != NULL) {
    g_dbus_connection_unexport_menu_model(priv->bus, *export_id);
    g_hash_table_remove (priv->menuModels, account_name);
    g_free(export_id);
  }
}

/**
 * cloud_provider_exporter_action_group:
 * @self: The cloud provider
 * @account_name: The name of the account
 * @action_group: The GActionGroup to be used by the menu exported by cloud_provider_exporter_export_menu
 *
 * In order for a menu exported with cloud_provider_exporter_export_menu to receive events
 * that will eventually call your callbacks, it needs the corresponding GAcionGroup.
 * Use this function to export it.
 */
guint
cloud_provider_exporter_export_action_group(CloudProviderExporter *self,
                                   const gchar   *account_name,
                                   GActionGroup  *action_group)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  gchar *object_path = g_strconcat(priv->object_path, "/", account_name, NULL);
  GError *error = NULL;
  guint *export_id = g_new0(guint, 1);
  *export_id = g_dbus_connection_export_action_group (priv->bus, object_path, action_group, &error);
  if (!*export_id)
    {
      g_warning ("Action export failed: %s", error->message);
      return 0;
    }
  g_hash_table_insert(priv->actionGroups, g_strdup(account_name), export_id);
  return *export_id;
}

/**
 * cloud_provider_exporter_unexport_action_group:
 * @self: The cloud provider
 * @account_name: The name of the account
 *
 * Unexport the GActionGroup exported by cloud_provider_exporter_export_action_group
 */
void
cloud_provider_exporter_unexport_action_group(CloudProviderExporter *self,
                                     const gchar   *account_name)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  guint *export_id;
  export_id = (guint*)g_hash_table_lookup(priv->actionGroups, account_name);
  if(export_id != NULL) {
    g_dbus_connection_unexport_action_group(priv->bus, *export_id);
    g_hash_table_remove (priv->actionGroups, account_name);
    g_free(export_id);
  }
}

/**
 * cloud_provider_exporter_export_objects:
 * @self: The cloud provider
 *
 * Export all objects assigned previously with functions like cloud_provider_exporter_export_account
 * to DBUS.
 * Use this function after exporting all the required object to avoid multiple signals
 * being emitted in a short time.
 */
void
cloud_provider_exporter_export_objects(CloudProviderExporter * self)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  g_dbus_object_manager_server_set_connection (priv->manager, priv->bus);
}

/**
 * cloud_provider_exporter_emit_changed:
 * @self: The cloud provider
 * @account_name: The name of the account
 *
 * When an account changes its status, emit a signal to DBUS using this function
 * so clients are aware of the change.
 */
void
cloud_provider_exporter_emit_changed (CloudProviderExporter *self,
                             const gchar   *account_name)
{
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private(self);
  gchar *object_path = g_strconcat(priv->object_path, "/", account_name, NULL);
  GDBusObject *object = g_dbus_object_manager_get_object((GDBusObjectManager*)priv->manager, object_path);
  CloudProviderAccount1 *account = cloud_provider_object_get_account1 (CLOUD_PROVIDER_OBJECT(object));
  cloud_provider_account1_emit_cloud_provider_changed (account);
  g_object_unref (account);
  g_object_unref (object);
  g_free (object_path);
}

CloudProviderExporter *
cloud_provider_exporter_new (GDBusConnection *bus,
                    const gchar     *bus_name,
                    const gchar     *object_path)
{
  CloudProviderExporter *self;
  CloudProviderExporterPrivate *priv;

  self = g_object_new (TYPE_CLOUD_PROVIDER_EXPORTER, NULL);
  priv = cloud_provider_exporter_get_instance_private (self);

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
cloud_provider_exporter_finalize (GObject *object)
{
  CloudProviderExporter *self = (CloudProviderExporter *)object;
  CloudProviderExporterPrivate *priv = cloud_provider_exporter_get_instance_private (self);

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

  G_OBJECT_CLASS (cloud_provider_exporter_parent_class)->finalize (object);
}

static void
cloud_provider_exporter_class_init (CloudProviderExporterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = cloud_provider_exporter_finalize;
}

static void
cloud_provider_exporter_init (CloudProviderExporter *self)
{
}
