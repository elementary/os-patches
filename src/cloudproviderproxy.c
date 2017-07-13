/* cloudproviderproxy.c
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

#include "cloudproviderproxy.h"
#include "cloudprovider-generated.h"


typedef struct
{
  gchar *name;
  gchar *path;
  CloudProviderStatus status;
  GIcon *icon;
  GMenuModel *menu_model;
  GActionGroup *action_group;

  GDBusConnection *bus;
  CloudProviderAccount1 *proxy;
  gchar *bus_name;
  gchar *object_path;
  GCancellable *cancellable;
  gboolean ready;
} CloudProviderProxyPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProviderProxy, cloud_provider_proxy, G_TYPE_OBJECT)

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
  CloudProviderProxy *self = CLOUD_PROVIDER_PROXY (user_data);
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);
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
  variant = g_variant_get_child_value (variant_dict, 0);
  priv->icon = g_icon_deserialize (variant_dict);
  g_variant_unref (variant);
  g_variant_unref (variant_dict);

out:
  g_variant_unref (variant_tuple);
  g_signal_emit_by_name (self, "changed");
  if(cloud_provider_proxy_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}

static void
on_get_name (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  CloudProviderProxy *self = CLOUD_PROVIDER_PROXY (user_data);
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);
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
  if(cloud_provider_proxy_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}


static void
on_get_path (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  CloudProviderProxy *self = CLOUD_PROVIDER_PROXY (user_data);
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);
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
  if(cloud_provider_proxy_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}

static void
on_get_status (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  CloudProviderProxy *self = CLOUD_PROVIDER_PROXY (user_data);
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);
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
  if(cloud_provider_proxy_is_available(self) && !priv->ready) {
    priv->ready = TRUE;
    g_signal_emit_by_name (self, "ready");
  }
}

void
cloud_provider_proxy_update (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

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
  CloudProviderProxy *self;
  CloudProviderProxyPrivate *priv;
  CloudProviderAccount1 *proxy;

  proxy = cloud_provider_account1_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error creating proxy for cloud provider %s", error->message);
      return;
    }
  self = CLOUD_PROVIDER_PROXY (user_data);
  priv = cloud_provider_proxy_get_instance_private (self);

  priv->proxy = proxy;

  g_signal_connect_swapped(priv->proxy, "cloud-provider-changed", G_CALLBACK(cloud_provider_proxy_update), self);

  cloud_provider_proxy_update(self);
}

static void
on_bus_acquired (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error = NULL;
  CloudProviderProxy *self;
  GDBusConnection *bus;
  CloudProviderProxyPrivate *priv;

  bus = g_bus_get_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error acdquiring bus for cloud provider %s", error->message);
      return;
    }

  self = CLOUD_PROVIDER_PROXY (user_data);
  priv = cloud_provider_proxy_get_instance_private (user_data);
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

CloudProviderProxy*
cloud_provider_proxy_new (const gchar *bus_name,
                        const gchar *object_path)
{
  CloudProviderProxy *self;
  CloudProviderProxyPrivate *priv;

  self = g_object_new (TYPE_CLOUD_PROVIDER_PROXY, NULL);
  priv = cloud_provider_proxy_get_instance_private (self);

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
cloud_provider_proxy_finalize (GObject *object)
{
  CloudProviderProxy *self = (CloudProviderProxy *)object;
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

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

  G_OBJECT_CLASS (cloud_provider_proxy_parent_class)->finalize (object);
}

static void
cloud_provider_proxy_class_init (CloudProviderProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_provider_proxy_finalize;

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
cloud_provider_proxy_init (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

  priv->status = CLOUD_PROVIDER_STATUS_INVALID;
}

gchar*
cloud_provider_proxy_get_name (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

  return priv->name;
}

CloudProviderStatus
cloud_provider_proxy_get_status (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

  return priv->status;
}

GIcon*
cloud_provider_proxy_get_icon (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

  return priv->icon;
}

GMenuModel*
cloud_provider_proxy_get_menu_model (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

  return priv->menu_model;
}

GActionGroup*
cloud_provider_proxy_get_action_group (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

  return priv->action_group;
}

gchar *
cloud_provider_proxy_get_path (CloudProviderProxy *self)
{
  CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

  return priv->path;
}

gchar *
cloud_provider_proxy_get_owner (CloudProviderProxy *self)
{
   CloudProviderProxyPrivate *priv = cloud_provider_proxy_get_instance_private (self);

   return g_dbus_proxy_get_name_owner (G_DBUS_PROXY(priv->proxy));
}

gboolean cloud_provider_proxy_is_available(CloudProviderProxy *self)
{
  GIcon *icon;
  gchar *name;
  gchar *path;
  guint status;

  name = cloud_provider_proxy_get_name (self);
  icon = cloud_provider_proxy_get_icon (self);
  status = cloud_provider_proxy_get_status (self);
  path = cloud_provider_proxy_get_path (self);
  if (name == NULL || icon == NULL || path == NULL || status == CLOUD_PROVIDER_STATUS_INVALID)
	  return FALSE;
  return TRUE;
}
