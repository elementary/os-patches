/*
 * Copyright 2010-2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of either or both of the following licenses:
 *
 * 1) the GNU Lesser General Public License version 3, as published by the
 * Free Software Foundation; and/or
 * 2) the GNU Lesser General Public License version 2.1, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the applicable version of the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License version 3 and version 2.1 along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 *
 * Authored by: Jason Smith <jason.smith@canonical.com>
                Robert Carr <racarr@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#include <libbamf-private/bamf-private.h>
#include "bamf-tab.h"
#include "bamf-view-private.h"

#define BAMF_TAB_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE (object, BAMF_TYPE_TAB, BamfTabPrivate))

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_DESKTOP_ID,
  PROP_XID,
  PROP_IS_FOREGROUND_TAB
};

struct _BamfTabPrivate
{
  BamfDBusItemTab *proxy;
};

static void bamf_tab_unset_proxy (BamfTab *self);

G_DEFINE_TYPE (BamfTab, bamf_tab, BAMF_TYPE_VIEW);

static void
on_proxy_property_change (GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
  BamfTab *self = BAMF_TAB (user_data);

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (self), pspec->name))
    g_object_notify (G_OBJECT (self), pspec->name);
}

static void
bamf_tab_set_path (BamfView *view, const gchar *path)
{
  BamfTab *self;
  BamfTabPrivate *priv;
  GError *error = NULL;

  self = BAMF_TAB (view);
  priv = self->priv;

  bamf_tab_unset_proxy (self);
  priv->proxy = _bamf_dbus_item_tab_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                            G_DBUS_PROXY_FLAGS_NONE,
                                                            BAMF_DBUS_SERVICE_NAME,
                                                            path, CANCELLABLE (view),
                                                            &error);
  if (!G_IS_DBUS_PROXY (priv->proxy))
    {
      g_error ("Unable to get %s tab: %s", BAMF_DBUS_SERVICE_NAME, error ? error->message : "");
      g_error_free (error);
      return;
    }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (priv->proxy), BAMF_DBUS_DEFAULT_TIMEOUT);
  g_signal_connect (priv->proxy, "notify", G_CALLBACK (on_proxy_property_change), self);
}

static void
bamf_tab_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  BamfTab *self;

  self = BAMF_TAB (object);

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
    }

  switch (property_id)
    {
    case PROP_LOCATION:
      g_value_set_string (value, _bamf_dbus_item_tab_get_location (self->priv->proxy));
      break;
    case PROP_DESKTOP_ID:
      g_value_set_string (value, _bamf_dbus_item_tab_get_desktop_id (self->priv->proxy));
      break;
    case PROP_XID:
      g_value_set_uint64 (value, _bamf_dbus_item_tab_get_xid (self->priv->proxy));
      break;
    case PROP_IS_FOREGROUND_TAB:
      g_value_set_boolean (value, _bamf_dbus_item_tab_get_is_foreground_tab (self->priv->proxy));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bamf_tab_unset_proxy (BamfTab *self)
{
  if (!G_IS_DBUS_PROXY (self->priv->proxy))
    return;

  g_signal_handlers_disconnect_by_data (self->priv->proxy, self);

  g_object_unref (self->priv->proxy);
  self->priv->proxy = NULL;
}

static void
bamf_tab_dispose (GObject *object)
{
  BamfTab *self;

  self = BAMF_TAB (object);

  bamf_tab_unset_proxy (self);

  if (G_OBJECT_CLASS (bamf_tab_parent_class)->dispose)
    G_OBJECT_CLASS (bamf_tab_parent_class)->dispose (object);
}

static void
bamf_tab_class_init (BamfTabClass *klass)
{
  GParamSpec *pspec;
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  BamfViewClass *view_class = BAMF_VIEW_CLASS (klass);

  obj_class->dispose = bamf_tab_dispose;
  obj_class->get_property = bamf_tab_get_property;

  view_class->set_path = bamf_tab_set_path;

  pspec = g_param_spec_string("location", "Location", "The Current location of the remote Tab",
                              NULL, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_LOCATION, pspec);

  pspec = g_param_spec_string("desktop-id", "Desktop Name", "The Desktop ID assosciated with the application hosted in the remote Tab",
                              NULL, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_DESKTOP_ID, pspec);

  pspec = g_param_spec_uint64("xid", "xid", "XID for the toplevel window containing the remote Tab",
                              0, G_MAXUINT64, 0, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_XID, pspec);

  pspec = g_param_spec_boolean("is-foreground-tab", "Foreground tab", "Whether the tab is the foreground tab in it's toplevel container",
                               FALSE, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_IS_FOREGROUND_TAB, pspec);

  g_type_class_add_private (obj_class, sizeof(BamfTabPrivate));
}

static void
bamf_tab_init (BamfTab *self)
{
  self->priv = BAMF_TAB_GET_PRIVATE (self);
}

BamfTab *
bamf_tab_new (const gchar *path)
{
  BamfTab *self;

  self = g_object_new (BAMF_TYPE_TAB, NULL);
  _bamf_view_set_path (BAMF_VIEW (self), path);

  return self;
}

/**
 * bamf_tab_raise:
 * @self: A #BamfTab.
 *
 * Selects the @self tab in the parent window.
 *
 * Returns: %TRUE if success, %FALSE otherwise.
 */
gboolean
bamf_tab_raise (BamfTab *self)
{
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_TAB (self), FALSE);

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    return FALSE;

  if (!_bamf_dbus_item_tab_call_raise_sync (self->priv->proxy, CANCELLABLE (self), &error))
    {
      g_warning ("Failed to invoke Raise method: %s", error ? error->message : "");
      g_error_free (error);

      return FALSE;
    }

  return TRUE;
}

/**
 * bamf_tab_close:
 * @self: A #BamfTab.
 *
 * Closes the selected @self tab.
 *
 * Returns: %TRUE if success, %FALSE otherwise.
 */
gboolean
bamf_tab_close (BamfTab *self)
{
  GError *error;

  g_return_val_if_fail (BAMF_IS_TAB (self), FALSE);

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    return FALSE;

  error = NULL;

  if (!_bamf_dbus_item_tab_call_close_sync (self->priv->proxy, CANCELLABLE (self), &error))
    {
      g_warning ("Failed to invoke Close method: %s", error->message);
      g_error_free (error);

      return FALSE;
    }

  return TRUE;
}

typedef struct _BamfTabPreviewRequestData
{
  BamfTab *self;
  BamfTabPreviewReadyCallback callback;
  gpointer user_data;
} BamfTabPreviewRequestData;

static void
on_preview_ready (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  BamfTab *self;
  BamfTabPreviewRequestData *data;
  gchar *preview_data = NULL;
  GError *error = NULL;

  data = user_data;
  self = data->self;

  if (_bamf_dbus_item_tab_call_request_preview_finish (self->priv->proxy, &preview_data, res, &error))
    {
      data->callback (self, preview_data, data->user_data);
      g_free (preview_data);
    }
  else
    {
      data->callback (self, NULL, data->user_data);

      g_warning ("Error requesting BamfTab preview: %s", error ? error->message : "");
      g_error_free (error);
    }

  g_free (data);
}

/**
 * bamf_tab_request_preview:
 * @self: a #BamfTab
 * @callback: (closure) (scope async): a callback function to call when the result is ready
 * @user_data: (closure) (allow-none): data to be sent to the callback.
 */
void
bamf_tab_request_preview (BamfTab *self, BamfTabPreviewReadyCallback callback, gpointer user_data)
{
  BamfTabPreviewRequestData *data;

  g_return_if_fail (BAMF_IS_TAB (self));
  g_return_if_fail (callback != NULL);

  data = g_malloc (sizeof (BamfTabPreviewRequestData));
  data->self = self;
  data->callback = callback;
  data->user_data = user_data;

  _bamf_dbus_item_tab_call_request_preview (self->priv->proxy, NULL,
                                            on_preview_ready, data);
}

const gchar *
bamf_tab_get_location (BamfTab *self)
{
  g_return_val_if_fail (BAMF_IS_TAB (self), NULL);

  if (BAMF_TAB_GET_CLASS (self)->get_location)
    return BAMF_TAB_GET_CLASS (self)->get_location (self);

  return _bamf_dbus_item_tab_get_location (self->priv->proxy);
}

/**
 * bamf_tab_get_desktop_name:
 * @self: A #BamfTab.
 *
 * Returns the desktop file for the tab.
 *
 * Returns: (transfer none): The tab desktop id or %NULL if not set or available. Do not free the returned value, it belongs to @self.
 */
const gchar *
bamf_tab_get_desktop_name (BamfTab *self)
{
  g_return_val_if_fail (BAMF_IS_TAB (self), NULL);

  if (BAMF_TAB_GET_CLASS (self)->get_desktop_name)
    return BAMF_TAB_GET_CLASS (self)->get_desktop_name (self);

  return _bamf_dbus_item_tab_get_desktop_id (self->priv->proxy);
}

/**
 * bamf_tab_get_xid:
 * @self: A #BamfTab.
 *
 * The desktop file for the tab.
 *
 * Returns: The tab parent window XID id or 0 if not set or available.
 */
guint64
bamf_tab_get_xid (BamfTab *self)
{
  g_return_val_if_fail (BAMF_IS_TAB (self), 0);

  if (BAMF_TAB_GET_CLASS (self)->get_xid)
    return BAMF_TAB_GET_CLASS (self)->get_xid (self);

  return _bamf_dbus_item_tab_get_xid (self->priv->proxy);
}

/**
 * bamf_tab_get_is_foreground_tab:
 * @self: A #BamfTab.
 *
 * Returns: %TRUE if the tab is the active one on parent window XID, %FALSE otherwise.
 */
gboolean
bamf_tab_get_is_foreground_tab (BamfTab *self)
{
  g_return_val_if_fail (BAMF_IS_TAB (self), FALSE);

  if (BAMF_TAB_GET_CLASS (self)->get_is_foreground_tab)
    return BAMF_TAB_GET_CLASS (self)->get_is_foreground_tab (self);

  return _bamf_dbus_item_tab_get_is_foreground_tab (self->priv->proxy);
}
