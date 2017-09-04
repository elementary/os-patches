/* cloudprovideraccount.c
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

#include "cloudprovideraccount.h"
#include "cloudprovider-generated.h"


typedef struct
{
  gchar *name;
  gchar *path;
  CloudProviderStatus status;
  gchar *status_details;
  GIcon *icon;
  GMenuModel *menu_model;
  GActionGroup *action_group;

  GDBusConnection *bus;
  CloudProviderAccount1 *proxy;
  gchar *bus_name;
  gchar *object_path;
  GCancellable *cancellable;
  gboolean ready;
} CloudProviderAccountPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProviderAccount, cloud_provider_account, G_TYPE_OBJECT)

/**
 * SECTION:cloudprovideraccount
 * @title: CloudProviderAccount
 * @short_description: Base object for representing a single account for clients.
 * @include: src/cloudprovideraccount.h
 *
 * #CloudProviderAccount is the basic object used to construct the integrator UI
 * and actions that a provider will present to the user, from the client side.
 * Integrators of the cloud providers can use this object to poll the
 * #CloudProvider menus, status and actions.
 */

enum {
  CHANGED,
  READY,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static void
on_get_icon (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  CloudProviderAccount *self = CLOUD_PROVIDER_ACCOUNT (user_data);
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  GError *error = NULL;
  GVariant *variant_tuple;
  GVariant *variant_dict;
  GVariant *variant;

  g_clear_object (&priv->icon);

  cloud_provider_account1_call_get_icon_finish (priv->proxy, &variant_tuple, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider icon %s", error->message);
      goto out;
    }

  variant_dict = g_variant_get_child_value (variant_tuple, 0);
    if (g_variant_is_of_type(variant_dict, G_VARIANT_TYPE_STRING))
    {
        priv->icon = g_icon_deserialize (variant_dict);
        g_variant_unref (variant_dict);
        goto out;
    }
  variant = g_variant_get_child_value (variant_dict, 0);
  priv->icon = g_icon_deserialize (variant_dict);
  g_variant_unref (variant);
  g_variant_unref (variant_dict);

out:
  g_variant_unref (variant_tuple);
  g_signal_emit_by_name (self, "changed");
  if(cloud_provider_account_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}

static void
on_get_name (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  CloudProviderAccount *self = CLOUD_PROVIDER_ACCOUNT (user_data);
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  GError *error = NULL;

  if (priv->name != NULL)
    g_free (priv->name);

  cloud_provider_account1_call_get_name_finish (priv->proxy, &priv->name, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider name %s", error->message);
      return;
    }
  g_signal_emit_by_name (self, "changed");
  if(cloud_provider_account_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}


static void
on_get_path (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  CloudProviderAccount *self = CLOUD_PROVIDER_ACCOUNT (user_data);
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  GError *error = NULL;

  if (priv->path != NULL)
    g_free (priv->path);

  cloud_provider_account1_call_get_path_finish (priv->proxy, &priv->path, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider name %s", error->message);
      return;
    }
  g_signal_emit_by_name (self, "changed");
  if(cloud_provider_account_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}

static void
on_get_status (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  CloudProviderAccount *self = CLOUD_PROVIDER_ACCOUNT (user_data);
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  GError *error = NULL;
  gint status;

  cloud_provider_account1_call_get_status_finish (priv->proxy, &status, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider name %s", error->message);
      return;
    }
  priv->status = status;
  g_signal_emit_by_name (self, "changed");
  if(cloud_provider_account_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}

static void
on_get_status_details(GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  CloudProviderAccount *self = CLOUD_PROVIDER_ACCOUNT (user_data);
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  GError *error = NULL;

  if (priv->status_details != NULL)
    g_free (priv->status_details);

  cloud_provider_account1_call_get_status_details_finish (priv->proxy, &priv->status_details, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the status details %s", error->message);
      return;
    }
  g_signal_emit_by_name (self, "changed");
  if(cloud_provider_account_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}

void
cloud_provider_account_update (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  if (priv->proxy != NULL)
    {
      cloud_provider_account1_call_get_name (priv->proxy,
                                         NULL,
                                         (GAsyncReadyCallback) on_get_name,
                                         self);
      cloud_provider_account1_call_get_status (priv->proxy,
                                         NULL,
                                         (GAsyncReadyCallback) on_get_status,
                                         self);
      cloud_provider_account1_call_get_status_details (priv->proxy,
                                         NULL,
                                         (GAsyncReadyCallback) on_get_status_details,
                                         self);
      cloud_provider_account1_call_get_icon (priv->proxy,
                                         NULL,
                                         (GAsyncReadyCallback) on_get_icon,
                                         self);
      cloud_provider_account1_call_get_path (priv->proxy,
                                         NULL,
                                         (GAsyncReadyCallback) on_get_path,
                                         self);

      priv->menu_model = (GMenuModel*) g_dbus_menu_model_get (priv->bus,
                                                              priv->bus_name,
                                                              priv->object_path);
      priv->action_group = (GActionGroup*) g_dbus_action_group_get (priv->bus,
                                                                    priv->bus_name,
                                                                    priv->object_path);
    }
}

static void
on_proxy_created (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GError *error = NULL;
  CloudProviderAccount *self;
  CloudProviderAccountPrivate *priv;
  CloudProviderAccount1 *proxy;

  proxy = cloud_provider_account1_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error creating proxy for cloud provider %s", error->message);
      return;
    }
  self = CLOUD_PROVIDER_ACCOUNT (user_data);
  priv = cloud_provider_account_get_instance_private (self);

  priv->proxy = proxy;

  g_signal_connect_swapped(priv->proxy, "cloud-provider-changed", G_CALLBACK(cloud_provider_account_update), self);

  cloud_provider_account_update(self);
}

static void
on_bus_acquired (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error = NULL;
  CloudProviderAccount *self;
  GDBusConnection *bus;
  CloudProviderAccountPrivate *priv;

  bus = g_bus_get_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error acdquiring bus for cloud provider %s", error->message);
      return;
    }

  self = CLOUD_PROVIDER_ACCOUNT (user_data);
  priv = cloud_provider_account_get_instance_private (user_data);
  priv->bus = bus;
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();
  cloud_provider_account1_proxy_new (priv->bus,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 priv->bus_name,
                                 priv->object_path,
                                 priv->cancellable,
                                 on_proxy_created,
                                 self);
}

/**
 * cloud_provider_account_new
 * @bus_name: DBus bus name
 * @object_path: Path to export the DBus object to
 *
 * A #CloudProviderAccount object are used to fetch details about cloud providers from DBus.
 * Object are usually fetched from cloud_providers_get_providers() as a list.
 */
CloudProviderAccount*
cloud_provider_account_new (const gchar *bus_name,
                            const gchar *object_path)
{
  CloudProviderAccount *self;
  CloudProviderAccountPrivate *priv;

  self = g_object_new (TYPE_CLOUD_PROVIDER_ACCOUNT, NULL);
  priv = cloud_provider_account_get_instance_private (self);

  priv->bus_name = g_strdup (bus_name);
  priv->object_path = g_strdup (object_path);
  priv->cancellable = g_cancellable_new ();
  priv->status = CLOUD_PROVIDER_STATUS_INVALID;
  priv->ready = FALSE;
  g_bus_get (G_BUS_TYPE_SESSION,
             priv->cancellable,
             on_bus_acquired,
             self);

  return self;
}

static void
cloud_provider_account_finalize (GObject *object)
{
  CloudProviderAccount *self = (CloudProviderAccount *)object;
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  g_free (priv->name);
  g_free (priv->path);
  g_clear_object (&priv->icon);
  g_clear_object (&priv->action_group);
  g_clear_object (&priv->bus);
  g_clear_object (&priv->proxy);
  g_free (priv->bus_name);
  g_free (priv->object_path);

  G_OBJECT_CLASS (cloud_provider_account_parent_class)->finalize (object);
}

static void
cloud_provider_account_class_init (CloudProviderAccountClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_provider_account_finalize;

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
  gSignals [READY] =
    g_signal_new ("ready",
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
cloud_provider_account_init (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  priv->status = CLOUD_PROVIDER_STATUS_INVALID;
}

/**
 * cloud_provider_account_get_name:
 * @self: A cloud provider account
 *
 * Get the name of the account
 *
 * Returns: The name of the cloud provider account
 */
gchar*
cloud_provider_account_get_name (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  return priv->name;
}

/**
 * cloud_provider_account_get_status:
 * @self: A cloud provider account
 *
 * Get the status of the account
 *
 * Returns: The status of the cloud provider account
 */
CloudProviderStatus
cloud_provider_account_get_status (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  return priv->status;
}

/**
 * cloud_provider_account_get_status_details:
 * @self: A cloud provider account
 *
 * Get the status details of the account
 *
 * Returns: The status detail description of the cloud provider account
 */
gchar*
cloud_provider_account_get_status_details (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  return priv->status_details;
}

/**
 * cloud_provider_account_get_icon:
 * @self: A cloud provider account
 *
 * Get the icon of the account
 *
 * Returns: The icon of the cloud provider account
 */
GIcon*
cloud_provider_account_get_icon (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  return priv->icon;
}

/**
 * cloud_provider_account_get_menu_model:
 * @self: A cloud provider account
 *
 * Get the menu model exported for the account
 *
 * Returns: The menu model exported by the cloud provider account
 */
GMenuModel*
cloud_provider_account_get_menu_model (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  return priv->menu_model;
}

/**
 * cloud_provider_account_get_action_group:
 * @self: A cloud provider account
 *
 * Get the action group exported in addition to the #GMenuModel from
 * cloud_provider_account_get_menu_model()
 *
 * Returns: The action group exported by the cloud provider account
 */
GActionGroup*
cloud_provider_account_get_action_group (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  return priv->action_group;
}

/**
 * cloud_provider_account_get_path:
 * @self: A cloud provider account
 *
 * Get the directory path of the account
 *
 * Returns: The directory path of the cloud provider account
 */
gchar *
cloud_provider_account_get_path (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  return priv->path;
}

gchar *
cloud_provider_account_get_owner (CloudProviderAccount *self)
{
   CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

   return g_dbus_proxy_get_name_owner (G_DBUS_PROXY(priv->proxy));
}

/**
 * cloud_provider_account_is_available:
 * @self: A cloud provider account
 *
 * Check if the account is ready to be used. This will check if name, icon, status
 * and path are set since it cannot be garantued otherwise during startup of the
 * cloud provider client.
 */
gboolean cloud_provider_account_is_available(CloudProviderAccount *self)
{
  GIcon *icon;
  gchar *name;
  gchar *path;
  guint status;

  name = cloud_provider_account_get_name (self);
  icon = cloud_provider_account_get_icon (self);
  status = cloud_provider_account_get_status (self);
  path = cloud_provider_account_get_path (self);
  if (name == NULL || icon == NULL || path == NULL || status == CLOUD_PROVIDER_STATUS_INVALID)
    return FALSE;
  return TRUE;
}
