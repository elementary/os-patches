/* cloudprovideraccount.c
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
#include "cloudprovideraccount.h"
#include "cloudprovider-generated.h"

typedef struct
{
  gchar *object_name;
  CloudProviderAccount1 *skeleton;
} CloudProviderAccountPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CloudProviderAccount, cloud_provider_account, G_TYPE_OBJECT)

gchar *
cloud_provider_account_get_object_name (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  return priv->object_name;
}

GDBusInterfaceSkeleton*
cloud_provider_account_get_skeleton (CloudProviderAccount *self)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  return G_DBUS_INTERFACE_SKELETON(priv->skeleton);
}

static void
on_get_name (CloudProviderAccount   *self,
             GDBusMethodInvocation  *invocation,
             gpointer                user_data)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  gchar *name;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT(self), "handle_get_name", &name);
  cloud_provider_account1_complete_get_name (priv->skeleton, invocation, name);
}

static void
on_get_icon (CloudProviderAccount   *self,
             GDBusMethodInvocation  *invocation,
             gpointer                user_data)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  GIcon *icon = NULL;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT(self), "handle_get_icon", &icon);
  cloud_provider_account1_complete_get_icon (priv->skeleton, invocation, g_variant_new("v", g_icon_serialize(icon)));
}

static void
on_get_path (CloudProviderAccount   *self,
             GDBusMethodInvocation  *invocation,
             gpointer                user_data)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  gchar *path = NULL;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT(self), "handle_get_path", &path);
  cloud_provider_account1_complete_get_path (priv->skeleton, invocation, path);
}

static void
on_get_status (CloudProviderAccount   *self,
               GDBusMethodInvocation  *invocation,
               gpointer                user_data)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  gint *status = g_new0(gint, 1);
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT(self), "handle_get_status", status);
  cloud_provider_account1_complete_get_status (priv->skeleton, invocation, *status);
  g_free(status);
}

static void
on_get_status_details (CloudProviderAccount   *self,
                       GDBusMethodInvocation  *invocation,
                       gpointer                user_data)
{
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);
  gchar *status_details = NULL;
  g_signal_emit_by_name (CLOUD_PROVIDER_ACCOUNT(self), "handle_get_status_details", &status_details);
  cloud_provider_account1_complete_get_status_details (priv->skeleton, invocation, status_details);
}

CloudProviderAccount*
cloud_provider_account_new (const gchar *object_name)
{
  CloudProviderAccount *self;
  CloudProviderAccountPrivate *priv;

  self = g_object_new (TYPE_CLOUD_PROVIDER_ACCOUNT, NULL);
  priv = cloud_provider_account_get_instance_private (self);

  priv->skeleton = cloud_provider_account1_skeleton_new ();

  g_signal_connect_swapped(priv->skeleton, "handle_get_name", G_CALLBACK (on_get_name), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_icon", G_CALLBACK (on_get_icon), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_path", G_CALLBACK (on_get_path), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_status", G_CALLBACK (on_get_status), self);
  g_signal_connect_swapped(priv->skeleton, "handle_get_status_details", G_CALLBACK (on_get_status_details), self);

  priv->object_name = g_strdup (object_name);

  return self;
}

static void
cloud_provider_account_finalize (GObject *object)
{
  CloudProviderAccount *self = (CloudProviderAccount *)object;
  CloudProviderAccountPrivate *priv = cloud_provider_account_get_instance_private (self);

  g_free (priv->object_name);
  g_object_unref (priv->skeleton);

  G_OBJECT_CLASS (cloud_provider_account_parent_class)->finalize (object);
}

static void
cloud_provider_account_class_init (CloudProviderAccountClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_provider_account_finalize;

  g_signal_new ("handle_get_name",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);
  g_signal_new ("handle_get_icon",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);
  g_signal_new ("handle_get_path",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);
  g_signal_new ("handle_get_status",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_POINTER,
                  0);
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
cloud_provider_account_init (CloudProviderAccount *self)
{
}
