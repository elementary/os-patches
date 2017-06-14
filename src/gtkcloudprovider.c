/* gtkcloudprovider.c
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

#include "gtkcloudprovider.h"

static const gchar provider_xml[] =
  "<node>"
  "  <interface name='org.gtk.CloudProvider'>"
  "    <method name='GetName'>"
  "      <arg type='s' name='name' direction='out'/>"
  "    </method>"
  "    <method name='GetStatus'>"
  "      <arg type='i' name='name' direction='out'/>"
  "    </method>"
  "    <method name='GetIcon'>"
  "      <arg type='v' name='icon' direction='out'/>"
  "    </method>"
  "    <method name='GetPath'>"
  "      <arg type='s' name='path' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";


typedef struct
{
  gchar *name;
  gchar *path;
  GtkCloudProviderStatus status;
  GIcon *icon;
  GMenuModel *menu_model;
  GActionGroup *action_group;

  GDBusConnection *bus;
  GDBusProxy *proxy;
  gchar *bus_name;
  gchar *object_path;
  GCancellable *cancellable;
} GtkCloudProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkCloudProvider, gtk_cloud_provider, G_TYPE_OBJECT)

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static void
on_get_icon (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  GtkCloudProvider *self = GTK_CLOUD_PROVIDER (user_data);
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);
  GError *error = NULL;
  GVariant *variant_tuple;
  GVariant *variant_dict;
  GVariant *variant;

  g_clear_object (&priv->icon);

  variant_tuple = g_dbus_proxy_call_finish (priv->proxy, res, &error);
  g_print ("variant tuple %s\n", g_variant_print (variant_tuple, TRUE));
  if (error != NULL)
    {
      g_warning ("Error getting the provider icon %s", error->message);
      goto out;
    }

  variant_dict = g_variant_get_child_value (variant_tuple, 0);
  variant = g_variant_get_child_value (variant_dict, 0);
  priv->icon = g_icon_deserialize (variant);
  g_variant_unref (variant);
  g_variant_unref (variant_dict);

out:
  g_variant_unref (variant_tuple);
  g_signal_emit_by_name (self, "changed");
}

static void
on_get_name (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  GtkCloudProvider *self = GTK_CLOUD_PROVIDER (user_data);
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);
  GError *error = NULL;
  GVariant *variant_tuple;
  GVariant *variant;

  if (priv->name != NULL)
    g_free (priv->name);

  variant_tuple = g_dbus_proxy_call_finish (priv->proxy, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider name %s", error->message);
      goto out;
    }

  variant = g_variant_get_child_value (variant_tuple, 0);
  priv->name = g_variant_dup_string (variant, NULL);
  g_variant_unref (variant);

out:
  g_variant_unref (variant_tuple);
  g_signal_emit_by_name (self, "changed");
}

static void
on_get_path (GObject      *source_object,
             GAsyncResult *res,
             gpointer      user_data)
{
  GtkCloudProvider *self = GTK_CLOUD_PROVIDER (user_data);
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);
  GError *error = NULL;
  GVariant *variant_tuple;
  GVariant *variant;

  if (priv->path != NULL)
    g_free (priv->path);

  variant_tuple = g_dbus_proxy_call_finish (priv->proxy, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider path %s", error->message);
      goto out;
    }

  variant = g_variant_get_child_value (variant_tuple, 0);
  priv->path = g_variant_dup_string (variant, NULL);
  g_variant_unref (variant);

out:
  g_variant_unref (variant_tuple);
  g_signal_emit_by_name (self, "changed");
}

static void
on_get_status (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GtkCloudProvider *self = GTK_CLOUD_PROVIDER (user_data);
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);
  GError *error = NULL;
  GVariant *variant_tuple;
  GVariant *variant;

  variant_tuple = g_dbus_proxy_call_finish (priv->proxy, res, &error);
  if (error != NULL)
    {
      g_warning ("Error getting the provider status %s", error->message);
      goto out;
    }

  variant = g_variant_get_child_value (variant_tuple, 0);
  priv->status = g_variant_get_int32 (variant);
  g_variant_unref (variant);

out:
  g_variant_unref (variant_tuple);
  g_signal_emit_by_name (self, "changed");
}

void
gtk_cloud_provider_update (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  if (priv->proxy != NULL)
    {
      g_dbus_proxy_call (priv->proxy,
                         "GetName",
                         g_variant_new ("()"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         (GAsyncReadyCallback) on_get_name,
                         self);

      g_dbus_proxy_call (priv->proxy,
                         "GetStatus",
                         g_variant_new ("()"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         (GAsyncReadyCallback) on_get_status,
                         self);

      g_dbus_proxy_call (priv->proxy,
                         "GetIcon",
                         g_variant_new ("()"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         (GAsyncReadyCallback) on_get_icon,
                         self);

      g_dbus_proxy_call (priv->proxy,
                         "GetPath",
                         g_variant_new ("()"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
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
  GtkCloudProvider *self;
  GtkCloudProviderPrivate *priv;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error creating proxy for cloud provider %s", error->message);
      return;
    }
  self = GTK_CLOUD_PROVIDER (user_data);
  priv = gtk_cloud_provider_get_instance_private (self);

  priv->proxy = proxy;

  gtk_cloud_provider_update (self);
}

static void
on_bus_acquired (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error = NULL;
  GtkCloudProvider *self;
  GDBusConnection *bus;
  GtkCloudProviderPrivate *priv;
  GDBusInterfaceInfo *interface_info;
  GDBusNodeInfo *proxy_info;

  bus = g_bus_get_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error acdquiring bus for cloud provider %s", error->message);
      return;
    }

  self = GTK_CLOUD_PROVIDER (user_data);
  priv = gtk_cloud_provider_get_instance_private (user_data);
  priv->bus = bus;
  proxy_info = g_dbus_node_info_new_for_xml (provider_xml, &error);
  interface_info = g_dbus_node_info_lookup_interface (proxy_info, "org.gtk.CloudProvider");
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();
  g_dbus_proxy_new (priv->bus,
                    G_DBUS_PROXY_FLAGS_NONE,
                    interface_info,
                    priv->bus_name,
                    priv->object_path,
                    "org.gtk.CloudProvider",
                    priv->cancellable,
                    on_proxy_created,
                    self);
}

GtkCloudProvider*
gtk_cloud_provider_new (const gchar *bus_name,
                        const gchar *object_path)
{
  GtkCloudProvider *self;
  GtkCloudProviderPrivate *priv;

  self = g_object_new (GTK_TYPE_CLOUD_PROVIDER, NULL);
  priv = gtk_cloud_provider_get_instance_private (self);

  priv->bus_name = g_strdup (bus_name);
  priv->object_path = g_strdup (object_path);
  priv->cancellable = g_cancellable_new ();
  priv->status = GTK_CLOUD_PROVIDER_STATUS_INVALID;
  g_bus_get (G_BUS_TYPE_SESSION,
             priv->cancellable,
             on_bus_acquired,
             self);

  return self;
}

static void
gtk_cloud_provider_finalize (GObject *object)
{
  GtkCloudProvider *self = (GtkCloudProvider *)object;
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

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

  G_OBJECT_CLASS (gtk_cloud_provider_parent_class)->finalize (object);
}

static void
gtk_cloud_provider_class_init (GtkCloudProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_cloud_provider_finalize;

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
gtk_cloud_provider_init (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  priv->status = GTK_CLOUD_PROVIDER_STATUS_INVALID;
}

gchar*
gtk_cloud_provider_get_name (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  return priv->name;
}

GtkCloudProviderStatus
gtk_cloud_provider_get_status (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  return priv->status;
}

GIcon*
gtk_cloud_provider_get_icon (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  return priv->icon;
}

GMenuModel*
gtk_cloud_provider_get_menu_model (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  return priv->menu_model;
}

GActionGroup*
gtk_cloud_provider_get_action_group (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  return priv->action_group;
}

gchar *
gtk_cloud_provider_get_path (GtkCloudProvider *self)
{
  GtkCloudProviderPrivate *priv = gtk_cloud_provider_get_instance_private (self);

  return priv->path;
}
