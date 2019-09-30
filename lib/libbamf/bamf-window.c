/*
 * Copyright 2010-2012 Canonical Ltd.
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
 *              Neil Jagdish Patel <neil.patel@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */
/**
 * SECTION:bamf-window
 * @short_description: The base class for all windows
 *
 * #BamfWindow is the base class that all windows need to derive from.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <libbamf-private/bamf-private.h>
#include "bamf-view-private.h"
#include "bamf-window.h"
#include "bamf-factory.h"

G_DEFINE_TYPE (BamfWindow, bamf_window, BAMF_TYPE_VIEW);

#define BAMF_WINDOW_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BAMF_TYPE_WINDOW, BamfWindowPrivate))

struct _BamfWindowPrivate
{
  BamfDBusItemWindow        *proxy;
  guint32                    xid;
  guint32                    pid;
  time_t                     last_active;
  gint                       monitor;
  BamfWindowType             type;
  BamfWindowMaximizationType maximized;
};

enum
{
  MONITOR_CHANGED,
  MAXIMIZED_CHANGED,

  LAST_SIGNAL,
};

static guint window_signals[LAST_SIGNAL] = { 0 };

time_t
bamf_window_last_active (BamfWindow *self)
{
  g_return_val_if_fail (BAMF_IS_WINDOW (self), 0);

  if (BAMF_WINDOW_GET_CLASS (self)->last_active)
    return BAMF_WINDOW_GET_CLASS (self)->last_active (self);

  return self->priv->last_active;
}

/**
 * bamf_window_get_transient:
 * @self: a #BamfWindow
 *
 * Returns: (transfer none) (allow-none): A transient for this #BamfWindow.
 */

BamfWindow *
bamf_window_get_transient (BamfWindow *self)
{
  BamfWindowPrivate *priv;
  BamfView *transient;
  char *path = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_WINDOW (self), FALSE);

  if (BAMF_WINDOW_GET_CLASS (self)->get_transient)
    return BAMF_WINDOW_GET_CLASS (self)->get_transient (self);

  priv = self->priv;

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    return NULL;

  if (!_bamf_dbus_item_window_call_transient_sync (priv->proxy, &path,
                                                   CANCELLABLE (self), &error))
    {
      g_warning ("Failed to fetch path: %s", error ? error->message : "");
      g_error_free (error);
      return NULL;
    }

  if (!path)
    return NULL;

  if (path[0] == '\0')
    {
      g_free (path);
      return NULL;
    }

  BamfFactory *factory = _bamf_factory_get_default ();
  transient = _bamf_factory_view_for_path_type (factory, path, BAMF_FACTORY_WINDOW);
  g_free (path);

  if (!BAMF_IS_WINDOW (transient))
    return NULL;

  return BAMF_WINDOW (transient);
}

BamfWindowType
bamf_window_get_window_type (BamfWindow *self)
{
  BamfWindowPrivate *priv;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_WINDOW (self), FALSE);

  if (BAMF_WINDOW_GET_CLASS (self)->get_window_type)
    return BAMF_WINDOW_GET_CLASS (self)->get_window_type (self);

  priv = self->priv;

  if (priv->type != BAMF_WINDOW_UNKNOWN)
    return priv->type;

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    return priv->type;

  if (!_bamf_dbus_item_window_call_window_type_sync (priv->proxy, &priv->type,
                                                     CANCELLABLE (self), &error))
    {
      priv->type = BAMF_WINDOW_UNKNOWN;
      g_warning ("Failed to fetch type: %s", error ? error->message : "");
      g_error_free (error);
    }

  return priv->type;
}

guint32
bamf_window_get_pid (BamfWindow *self)
{
  BamfWindowPrivate *priv;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_WINDOW (self), FALSE);

  if (BAMF_WINDOW_GET_CLASS (self)->get_pid)
    return BAMF_WINDOW_GET_CLASS (self)->get_pid (self);

  priv = self->priv;

  if (priv->pid != 0)
    return priv->pid;

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    return priv->pid;

  if (!_bamf_dbus_item_window_call_get_pid_sync (priv->proxy, &priv->pid,
                                                 CANCELLABLE (self), &error))
    {
      priv->pid = 0;
      g_warning ("Failed to fetch pid: %s", error ? error->message : "");
      g_error_free (error);
    }

  return priv->pid;
}

guint32
bamf_window_get_xid (BamfWindow *self)
{
  BamfWindowPrivate *priv;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_WINDOW (self), FALSE);

  if (BAMF_WINDOW_GET_CLASS (self)->get_xid)
    return BAMF_WINDOW_GET_CLASS (self)->get_xid (self);

  priv = self->priv;

  if (priv->xid != 0)
    return priv->xid;

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    return priv->xid;

  if (!_bamf_dbus_item_window_call_get_xid_sync (priv->proxy, &priv->xid,
                                                 CANCELLABLE (self), &error))
    {
      priv->xid = 0;
      g_warning ("Failed to fetch xid: %s", error ? error->message : "");
      g_error_free (error);
    }

  return priv->xid;
}

gchar *
bamf_window_get_utf8_prop (BamfWindow *self, const char* xprop)
{
  BamfWindowPrivate *priv;
  char *result = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_WINDOW (self), NULL);
  g_return_val_if_fail (xprop, NULL);

  if (BAMF_WINDOW_GET_CLASS (self)->get_utf8_prop)
    return BAMF_WINDOW_GET_CLASS (self)->get_utf8_prop (self, xprop);

  priv = self->priv;

  if (!_bamf_view_remote_ready (BAMF_VIEW (self)))
    return NULL;

  if (!_bamf_dbus_item_window_call_xprop_sync (priv->proxy, xprop, &result,
                                               CANCELLABLE (self), &error))
    {
      g_warning ("Failed to fetch property `%s': %s", xprop, error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  if (result && result[0] == '\0')
    {
      g_free (result);
      result = NULL;
    }

  return result;
}

gint
bamf_window_get_monitor (BamfWindow *self)
{
  BamfWindowPrivate *priv;
  gint monitor = -2;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_WINDOW (self), -1);

  if (BAMF_WINDOW_GET_CLASS (self)->get_monitor)
    return BAMF_WINDOW_GET_CLASS (self)->get_monitor (self);

  priv = self->priv;

  if (priv->monitor != -2 || !_bamf_view_remote_ready (BAMF_VIEW (self)))
    {
      return priv->monitor;
    }

  if (!_bamf_dbus_item_window_call_monitor_sync (priv->proxy, &monitor,
                                                 CANCELLABLE (self), &error))
    {
      g_warning ("Failed to fetch monitor: %s", error ? error->message : "");
      g_error_free (error);

      monitor = -1;
    }

  return monitor;
}

BamfWindowMaximizationType
bamf_window_maximized (BamfWindow *self)
{
  BamfWindowPrivate *priv;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_WINDOW (self), -1);

  if (BAMF_WINDOW_GET_CLASS (self)->maximized)
    return BAMF_WINDOW_GET_CLASS (self)->maximized (self);

  priv = self->priv;

  if (priv->maximized != -1 || !_bamf_view_remote_ready (BAMF_VIEW (self)))
    {
      return priv->maximized;
    }

  if (!_bamf_dbus_item_window_call_maximized_sync (priv->proxy, (gint*) &priv->maximized,
                                                   CANCELLABLE (self), &error))
    {
      priv->maximized = -1;
      g_warning ("Failed to fetch maximized state: %s", error->message);
      g_error_free (error);
    }

  return priv->maximized;
}

static void
bamf_window_active_changed (BamfView *view, gboolean active)
{
  BamfWindow *self;

  g_return_if_fail (BAMF_IS_WINDOW (view));

  self = BAMF_WINDOW (view);

  if (active)
    self->priv->last_active = time (NULL);
}

static void
bamf_window_on_monitor_changed (BamfDBusItemWindow *proxy, gint old, gint new, BamfWindow *self)
{
  self->priv->monitor = new;
  g_signal_emit (G_OBJECT (self), window_signals[MONITOR_CHANGED], 0, old, new);
}

static void
bamf_window_on_maximized_changed (BamfDBusItemWindow *proxy, gint old, gint new, BamfWindow *self)
{
  self->priv->maximized = new;
  g_signal_emit (G_OBJECT (self), window_signals[MAXIMIZED_CHANGED], 0, old, new);
}

static void
bamf_window_unset_proxy (BamfWindow *self)
{
  BamfWindowPrivate *priv;

  g_return_if_fail (BAMF_IS_WINDOW (self));
  priv = self->priv;

  if (!G_IS_DBUS_PROXY (priv->proxy))
    return;

  g_signal_handlers_disconnect_by_data (priv->proxy, self);

  g_object_unref (priv->proxy);
  priv->proxy = NULL;
}

static void
bamf_window_set_path (BamfView *view, const char *path)
{
  BamfWindow *self;
  BamfWindowPrivate *priv;
  GError *error = NULL;

  self = BAMF_WINDOW (view);
  priv = self->priv;

  bamf_window_unset_proxy (self);
  priv->proxy = _bamf_dbus_item_window_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                               G_DBUS_PROXY_FLAGS_NONE,
                                                               BAMF_DBUS_SERVICE_NAME,
                                                               path, CANCELLABLE (self),
                                                               &error);
  if (!G_IS_DBUS_PROXY (priv->proxy))
    {
      g_error ("Unable to get %s window: %s", BAMF_DBUS_SERVICE_NAME, error ? error->message : "");
      g_error_free (error);
      return;
    }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (priv->proxy), BAMF_DBUS_DEFAULT_TIMEOUT);

  priv->xid = bamf_window_get_xid (self);
  priv->type = bamf_window_get_window_type (self);
  priv->monitor = bamf_window_get_monitor (self);
  priv->maximized = bamf_window_maximized (self);

  g_signal_connect (priv->proxy, "monitor-changed",
                    G_CALLBACK (bamf_window_on_monitor_changed), self);

  g_signal_connect (priv->proxy, "maximized-changed",
                    G_CALLBACK (bamf_window_on_maximized_changed), self);
}

static void
bamf_window_dispose (GObject *object)
{
  BamfWindow *self = BAMF_WINDOW (object);
  bamf_window_unset_proxy (self);

  if (G_OBJECT_CLASS (bamf_window_parent_class)->dispose)
    G_OBJECT_CLASS (bamf_window_parent_class)->dispose (object);
}

static void
bamf_window_class_init (BamfWindowClass *klass)
{
  GObjectClass  *obj_class  = G_OBJECT_CLASS (klass);
  BamfViewClass *view_class = BAMF_VIEW_CLASS (klass);

  g_type_class_add_private (obj_class, sizeof (BamfWindowPrivate));

  obj_class->dispose = bamf_window_dispose;
  view_class->active_changed = bamf_window_active_changed;
  view_class->set_path = bamf_window_set_path;

  window_signals[MONITOR_CHANGED] =
    g_signal_new (BAMF_WINDOW_SIGNAL_MONITOR_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfWindowClass, monitor_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT, G_TYPE_INT);

  window_signals[MAXIMIZED_CHANGED] =
    g_signal_new (BAMF_WINDOW_SIGNAL_MAXIMIZED_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfWindowClass, maximized_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT, G_TYPE_INT);
}

static void
bamf_window_init (BamfWindow *self)
{
  BamfWindowPrivate *priv;

  priv = self->priv = BAMF_WINDOW_GET_PRIVATE (self);
  priv->xid = 0;
  priv->pid = 0;
  priv->type = BAMF_WINDOW_UNKNOWN;
  priv->monitor = -2;
  priv->maximized = -1;
}

BamfWindow *
bamf_window_new (const char * path)
{
  BamfWindow *self = g_object_new (BAMF_TYPE_WINDOW, NULL);
  _bamf_view_set_path (BAMF_VIEW (self), path);

  return self;
}
