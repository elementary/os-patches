/* cloudproviders.c
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

#include "cloudproviders.h"
#include "cloudproviderproxy.h"
#include "cloudprovidermanager.h"
#include "cloudprovidermanager-generated.h"
#include "cloudprovider-generated.h"
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
  GDBusConnection *bus;
  CloudProviderManager1 *proxy;
  GCancellable *cancellable;
} CloudProvidersPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProviders, cloud_providers, G_TYPE_OBJECT)

enum
{
  CHANGED,
  OWNERS_CHANGED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static void
on_cloud_provider_proxy_ready (CloudProviderProxy *cloud_provider, CloudProviders *self)
{
  // notify clients that cloud provider list has changed
   g_signal_emit_by_name (self, "owners-changed", NULL);
}

static void
on_cloud_provider_changed (CloudProviderProxy *cloud_provider, CloudProviders *self)
{
  // notify clients that cloud provider has changed
  g_signal_emit_by_name (self, "changed", NULL);
}

/**
 * Update provider list if objects are added/removed at object manager.
 */
static void
on_cloud_provider_object_manager_notify (GObject    *object,
                                         GParamSpec *pspec,
                                         gpointer user_data)
{
  CloudProviders *self = CLOUD_PROVIDERS(user_data);
  cloud_providers_update(self);
}

static void
on_proxy_created (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GError *error = NULL;
  CloudProviders *self;
  CloudProvidersPrivate *priv;
  CloudProviderManager1 *proxy;

  proxy = cloud_provider_manager1_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error creating proxy for cloud provider %s", error->message);
      return;
    }
  self = CLOUD_PROVIDERS (user_data);
  priv = cloud_providers_get_instance_private (self);

  priv->proxy = proxy;

  cloud_providers_update(self);
}

static void
on_bus_acquired (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error = NULL;
  CloudProviders *self;
  GDBusConnection *bus;
  CloudProvidersPrivate *priv;

  bus = g_bus_get_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error acdquiring bus for cloud provider %s", error->message);
      return;
    }

  self = CLOUD_PROVIDERS (user_data);
  priv = cloud_providers_get_instance_private (user_data);
  priv->bus = bus;
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();
  cloud_provider_manager1_proxy_new (priv->bus,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     CLOUD_PROVIDER_MANAGER_DBUS_NAME,
                                     CLOUD_PROVIDER_MANAGER_DBUS_PATH,
                                     priv->cancellable,
                                     on_proxy_created,
                                     self);
}

/**
 * cloud_providers_dup_singleton
 * Returns: (transfer none): A manager singleton
 */
CloudProviders *
cloud_providers_dup_singleton (void)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      CloudProvidersPrivate *priv;
      self = g_object_new (TYPE_CLOUD_PROVIDERS, NULL);
      priv = cloud_providers_get_instance_private (CLOUD_PROVIDERS(self));
      priv->provider_object_managers = g_hash_table_new(g_str_hash, g_str_equal);

      g_bus_get (G_BUS_TYPE_SESSION,
                  priv->cancellable,
                  on_bus_acquired,
                  self);

      return CLOUD_PROVIDERS (self);
    }
  else
    {
      return g_object_ref (self);
    }
}

static void
cloud_providers_finalize (GObject *object)
{
  CloudProviders *self = (CloudProviders *)object;
  CloudProvidersPrivate *priv = cloud_providers_get_instance_private (self);

  g_list_free_full (priv->providers, g_object_unref);
  g_bus_unown_name (priv->dbus_owner_id);
  g_dbus_node_info_unref (priv->dbus_node_info);

  G_OBJECT_CLASS (cloud_providers_parent_class)->finalize (object);
}

static void
cloud_providers_class_init (CloudProvidersClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_providers_finalize;

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
cloud_providers_init (CloudProviders *self)
{
}

/**
 * cloud_providers_get_providers
 * @self: A CloudProviders
 * Returns: (transfer none): A GList* of #CloudProviderProxy objects.
 */
GList*
cloud_providers_get_providers (CloudProviders *self)
{
  CloudProvidersPrivate *priv = cloud_providers_get_instance_private (self);
  return priv->providers;
}


static void
on_get_cloud_providers (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  CloudProviders *self = CLOUD_PROVIDERS (user_data);
  CloudProvidersPrivate *priv = cloud_providers_get_instance_private (self);
  GError *error = NULL;
  GVariant *foo;
  CloudProviderProxy*cloud_provider;
  gboolean success = FALSE;
  GVariantIter iter;
  gchar *bus_name;
  gchar *object_path;

  cloud_provider_manager1_call_get_cloud_providers_finish (priv->proxy, &foo, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider name %s", error->message);
      return;
    }

  g_list_free_full (priv->providers, g_object_unref);
  priv->providers = NULL;

  g_variant_iter_init (&iter, foo);
  while (g_variant_iter_next (&iter, "(so)", &bus_name, &object_path))
    {
      GList *objects;
      GList *l;
      GDBusObjectManager *manager = g_hash_table_lookup(priv->provider_object_managers, bus_name);
      if (manager == NULL)
        {
          manager = cloud_provider_object_manager_client_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                     G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                     bus_name,
                                                     object_path,
                                                     NULL,
                                                     &error);

          if (error != NULL)
            {
              g_printerr ("Error getting object manager client: %s", error->message);
              g_error_free (error);
              goto out;
            }

          g_signal_connect(manager, "notify::name-owner", G_CALLBACK(on_cloud_provider_object_manager_notify), self);
          g_signal_connect(manager, "object-added", G_CALLBACK(on_cloud_provider_object_manager_notify), self);
          g_signal_connect(manager, "object-removed", G_CALLBACK(on_cloud_provider_object_manager_notify), self);
          g_hash_table_insert(priv->provider_object_managers, bus_name, manager);
        }
      objects = g_dbus_object_manager_get_objects (manager);
      for (l = objects; l != NULL; l = l->next)
        {
          CloudProviderObject *object = CLOUD_PROVIDER_OBJECT(l->data);
          cloud_provider = cloud_provider_proxy_new (bus_name, g_dbus_object_get_object_path (G_DBUS_OBJECT (object)));
          g_signal_connect (cloud_provider, "ready",
                            G_CALLBACK (on_cloud_provider_proxy_ready), self);
          g_signal_connect (cloud_provider, "changed",
                            G_CALLBACK (on_cloud_provider_changed), self);
          cloud_provider_proxy_update (cloud_provider);
          priv->providers = g_list_append (priv->providers, cloud_provider);
        }
    }
    success = TRUE;
    g_signal_emit_by_name (self, "owners-changed", NULL);
out:
  if (!success)
    g_warning ("Error while loading cloud providers");

}

/**
 * cloud_providers_update
 * @manager: A CloudProviders
 */
void
cloud_providers_update (CloudProviders *manager)
{
  CloudProvidersPrivate *priv = cloud_providers_get_instance_private (manager);
  if(priv->proxy == NULL)
    return;
  cloud_provider_manager1_call_get_cloud_providers (priv->proxy,
                                         NULL,
                                         (GAsyncReadyCallback) on_get_cloud_providers,
                                         manager);
}

