/* cloudprovideraccountexporter.c
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

#include <gio/gio.h>
#include "cloudprovideraccountexporter.h"
#include "cloudprovideraccountexporterpriv.h"
#include "cloudprovider-generated.h"

typedef struct
{
  gchar *object_name;
  CloudProviderAccount1 *skeleton;
  GMenuModel *menu_model;
  GActionGroup *action_group;
} CloudProviderAccountExporterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProviderAccountExporter, cloud_provider_account_exporter, G_TYPE_OBJECT)

/**
 * SECTION:cloudprovideraccountexporter
 * @title: CloudProviderAccountExporter
 * @short_description: Base object for representing a cloud providers account
 * @include: src/cloudprovideraccountexporter.h
 */

gchar *
cloud_provider_account_exporter_get_object_name (CloudProviderAccountExporter *self)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  return priv->object_name;
}

GMenuModel *
cloud_provider_account_exporter_get_menu_model (CloudProviderAccountExporter *self)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  return priv->menu_model;
}

GActionGroup *
cloud_provider_account_exporter_get_action_group (CloudProviderAccountExporter *self)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  return priv->action_group;
}

GDBusInterfaceSkeleton*
cloud_provider_account_exporter_get_skeleton (CloudProviderAccountExporter *self)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  return G_DBUS_INTERFACE_SKELETON(priv->skeleton);
}

static void
on_get_name (CloudProviderAccountExporter   *self,
             GDBusMethodInvocation          *invocation,
             gpointer                        user_data)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  gchar *name;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT_EXPORTER(self), "handle_get_name", &name);
  cloud_provider_account1_complete_get_name (priv->skeleton, invocation, name);
}

static void
on_get_icon (CloudProviderAccountExporter   *self,
             GDBusMethodInvocation          *invocation,
             gpointer                        user_data)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  GIcon *icon = NULL;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT_EXPORTER(self), "handle_get_icon", &icon);
  cloud_provider_account1_complete_get_icon (priv->skeleton, invocation, g_variant_new("v", g_icon_serialize(icon)));
}

static void
on_get_path (CloudProviderAccountExporter   *self,
             GDBusMethodInvocation          *invocation,
             gpointer                        user_data)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  gchar *path = NULL;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT_EXPORTER(self), "handle_get_path", &path);
  cloud_provider_account1_complete_get_path (priv->skeleton, invocation, path);
}

static void
on_get_status (CloudProviderAccountExporter   *self,
               GDBusMethodInvocation          *invocation,
               gpointer                        user_data)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  gint *status = g_new0(gint, 1);
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT_EXPORTER(self), "handle_get_status", status);
  cloud_provider_account1_complete_get_status (priv->skeleton, invocation, *status);
  g_free(status);
}

static void
on_get_status_details (CloudProviderAccountExporter   *self,
                       GDBusMethodInvocation          *invocation,
                       gpointer                        user_data)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  gchar *status_details = NULL;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT_EXPORTER(self), "handle_get_status_details", &status_details);
  cloud_provider_account1_complete_get_status_details (priv->skeleton, invocation, status_details);
}

/**
 * cloud_provider_account_exporter_new:
 * @object_name: A unique name for the account
 *               must be a valid DBus object name
 *
 * Create a new #CloudProviderAccountExporter object
 */
CloudProviderAccountExporter*
cloud_provider_account_exporter_new (const gchar *object_name)
{
  CloudProviderAccountExporter *self;
  CloudProviderAccountExporterPrivate *priv;

  self = g_object_new (TYPE_CLOUD_PROVIDER_ACCOUNT_EXPORTER, NULL);
  priv = cloud_provider_account_exporter_get_instance_private (self);

  priv->skeleton = cloud_provider_account1_skeleton_new ();

  g_signal_connect_swapped(priv->skeleton, "handle_get_name", G_CALLBACK (on_get_name), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_icon", G_CALLBACK (on_get_icon), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_path", G_CALLBACK (on_get_path), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_status", G_CALLBACK (on_get_status), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_status_details", G_CALLBACK (on_get_status_details), self);

  priv->object_name = g_strdup (object_name);

  return self;
}

/**
 * cloud_provider_account_exporter_add_menu_model:
 * @self: The cloud provider account exporter
 * @menu_model: The g menu model to export
 *
 * Add a #GMenuModel to export via DBus which will be shown to the user
 * for actions with the cloud provider account
 */
void
cloud_provider_account_exporter_add_menu_model (CloudProviderAccountExporter *self,
                                                GMenuModel                   *menu_model)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  priv->menu_model = menu_model;
}

/**
 * cloud_provider_account_exporter_remove_menu:
 * @self: The cloud provider account exporter
 *
 * Remove a menu added with cloud_provider_account_exporter_add_menu_model()
 */
void
cloud_provider_account_exporter_remove_menu (CloudProviderAccountExporter *self)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  priv->menu_model = NULL;
}

/**
 * cloud_provider_account_exporter_add_action_group:
 * @self: The cloud provider account exporter
 * @action_group: The g action group to export
 *
 * Add a #GActionGroup to export via DBus to provide actions for menus exported
 * with #cloud_provider_account_exporter_add_menu()
 */
void
cloud_provider_account_exporter_add_action_group (CloudProviderAccountExporter *self,
                                                  GActionGroup                 *action_group)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  priv->action_group = action_group;
}

/**
 * cloud_provider_account_exporter_remove_action_group:
 * @self: The cloud provider account exporter
 *
 * Remove an action group added with cloud_provider_account_exporter_add_action_group()
 */
void
cloud_provider_account_exporter_remove_action_group (CloudProviderAccountExporter *self)
{
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);
  priv->action_group = NULL;
}

static void
cloud_provider_account_exporter_finalize (GObject *object)
{
  CloudProviderAccountExporter *self = (CloudProviderAccountExporter *)object;
  CloudProviderAccountExporterPrivate *priv = cloud_provider_account_exporter_get_instance_private (self);

  g_free (priv->object_name);
  g_object_unref (priv->skeleton);

  G_OBJECT_CLASS (cloud_provider_account_exporter_parent_class)->finalize (object);
}

static void
cloud_provider_account_exporter_class_init (CloudProviderAccountExporterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_provider_account_exporter_finalize;

  /**
   * CloudProviderAccountExporter::handle-get-name:
   * @self: The CloudProviderAccountExporter emitting the signal
   *
   * This signal is emitted each time someone tries to get the account name.
   *
   * Returns: Return a #gchar* in the signal handler
   */
  g_signal_new ("handle-get-name",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_generic,
                G_TYPE_POINTER,
                0);

  /**
   * CloudProviderAccountExporter::handle-get-icon
   * @self: The CloudProviderAccountExporter emitting the signal
   *
   * This signal is emitted each time someone tries to get the account name.
   *
   * Returns: Return a #GIcon* in the signal handler
   */
  g_signal_new ("handle_get_icon",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);

  /**
   * CloudProviderAccountExporter::handle-get-path
   * @self: The #CloudProviderAccountExporter emitting the signal
   *
   * This signal is emitted each time someone tries to get the path.
   *
   * Returns: Return a #gchar* in the signal handler
   */
  g_signal_new ("handle_get_path",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);

  /**
   * CloudProviderAccountExporter::handle-get-status
   * @self: The CloudProviderAccountExporter emitting the signal
   *
   * This signal is emitted each time someone tries to get the status.
   *
   * Returns: Return a #CloudProviderSyncStatus in the signal handler
   */
  g_signal_new ("handle_get_status",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);

  /**
   * CloudProviderAccountExporter::handle-get-status-details
   * @self: The #CloudProviderAccountExporter emitting the signal
   *
   * This signal is emitted each time someone tries to get the status details.
   *
   * Returns: Return a #gchar* in the signal handler
   */
  g_signal_new ("handle_get_status_details",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);
}

static void
cloud_provider_account_exporter_init (CloudProviderAccountExporter *self)
{
}
