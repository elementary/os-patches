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
 * SECTION:bamf-view
 * @short_description: The base class for all views
 *
 * #BamfView is the base class that all views need to derive from.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <libbamf-private/bamf-private.h>

#include "bamf-view.h"
#include "bamf-view-private.h"
#include "bamf-factory.h"
#include "bamf-tab.h"
#include "bamf-window.h"

G_DEFINE_TYPE (BamfView, bamf_view, G_TYPE_INITIALLY_UNOWNED);

#define BAMF_VIEW_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BAMF_TYPE_VIEW, BamfViewPrivate))

enum
{
  ACTIVE_CHANGED,
  CLOSED,
  CHILD_ADDED,
  CHILD_REMOVED,
  CHILD_MOVED,
  RUNNING_CHANGED,
  URGENT_CHANGED,
  VISIBLE_CHANGED,
  NAME_CHANGED,
  ICON_CHANGED,
  LAST_SIGNAL
};

enum
{
  PROP_0,

  PROP_PATH,
  PROP_RUNNING,
  PROP_ACTIVE,
  PROP_USER_VISIBLE,
  PROP_URGENT,
  PROP_LAST
};

static guint view_signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[PROP_LAST] = { 0 };

struct _BamfViewPrivate
{
  BamfDBusItemView *proxy;
  GCancellable     *cancellable;
  gchar            *type;
  gchar            *cached_name;
  gchar            *cached_icon;
  GList            *cached_children;
  gboolean          reload_children;
  gboolean          is_closed;
  gboolean          sticky;
};

static void bamf_view_unset_proxy (BamfView *self);

/**
 * bamf_view_get_children:
 * @view: a #BamfView
 *
 * Note: Makes sever dbus calls the first time this is called on a view. Dbus messaging is reduced afterwards.
 *
 * Returns: (element-type Bamf.View) (transfer container): Returns a list of #BamfView which must be
 *           freed after usage. Elements of the list are owned by bamf and should not be unreffed.
 */
GList *
bamf_view_get_children (BamfView *view)
{
  char ** children;
  int i, len;
  GList *results = NULL;
  GError *error = NULL;
  BamfViewPrivate *priv;
  BamfView *child;

  g_return_val_if_fail (BAMF_IS_VIEW (view), NULL);

  if (BAMF_VIEW_GET_CLASS (view)->get_children)
    return BAMF_VIEW_GET_CLASS (view)->get_children (view);

  if (!_bamf_view_remote_ready (view))
    return NULL;

  priv = view->priv;

  if (priv->cached_children || !priv->reload_children)
    return g_list_copy (priv->cached_children);

  if (!_bamf_dbus_item_view_call_children_sync (priv->proxy, &children, CANCELLABLE (view), &error))
    {
      g_warning ("Unable to fetch children: %s\n", error ? error->message : "");
      g_error_free (error);
      return NULL;
    }

  if (!children)
    return NULL;

  len = g_strv_length (children);

  for (i = len-1; i >= 0; --i)
    {
      child = _bamf_factory_view_for_path (_bamf_factory_get_default (), children[i]);

      if (BAMF_IS_VIEW (child))
        {
          results = g_list_prepend (results, g_object_ref (child));
        }
    }

  if (priv->cached_children)
    g_list_free_full (priv->cached_children, g_object_unref);

  priv->reload_children = FALSE;
  priv->cached_children = results;

  return g_list_copy (priv->cached_children);
}

/**
 * bamf_view_is_closed:
 * @view: a #BamfView
 *
 * Determines if the view is closed or not.
 */
gboolean
bamf_view_is_closed (BamfView *view)
{
  g_return_val_if_fail (BAMF_IS_VIEW (view), TRUE);

  return view->priv->is_closed;
}

/**
 * bamf_view_is_active:
 * @view: a #BamfView
 *
 * Determines if the view is currently active and focused by the user. Useful for an active window indicator.
 */
gboolean
bamf_view_is_active (BamfView *view)
{
  g_return_val_if_fail (BAMF_IS_VIEW (view), FALSE);

  if (BAMF_VIEW_GET_CLASS (view)->is_active)
    return BAMF_VIEW_GET_CLASS (view)->is_active (view);

  if (!_bamf_view_remote_ready (view))
    return FALSE;

  return _bamf_dbus_item_view_get_active (view->priv->proxy);
}

/**
 * bamf_view_is_user_visible:
 * @view: a #BamfView
 *
 * Returns: a boolean useful for determining if a particular view is "user visible". User visible
 * is a concept relating to whether or not a window should be shown in a launcher tasklist.
 *
 * Since: 0.4.0
 */
gboolean
bamf_view_is_user_visible (BamfView *self)
{
  g_return_val_if_fail (BAMF_IS_VIEW (self), FALSE);

  if (BAMF_VIEW_GET_CLASS (self)->is_user_visible)
    return BAMF_VIEW_GET_CLASS (self)->is_user_visible (self);

  if (!_bamf_view_remote_ready (self))
    return FALSE;

  return _bamf_dbus_item_view_get_user_visible (self->priv->proxy);
}

/**
 * bamf_view_user_visible: (skip)
 * @view: a #BamfView
 *
 * Returns: a boolean useful for determining if a particular view is "user visible". User visible
 * is a concept relating to whether or not a window should be shown in a launcher tasklist.
 *
 * Deprecated: 0.4.0
 */
gboolean
bamf_view_user_visible (BamfView *self)
{
  return bamf_view_is_user_visible (self);
}

/**
 * bamf_view_is_running:
 * @view: a #BamfView
 *
 * Determines if the view is currently running. Useful for a running window indicator.
 */
gboolean
bamf_view_is_running (BamfView *self)
{
  g_return_val_if_fail (BAMF_IS_VIEW (self), FALSE);

  if (BAMF_VIEW_GET_CLASS (self)->is_running)
    return BAMF_VIEW_GET_CLASS (self)->is_running (self);

  if (!_bamf_view_remote_ready (self))
    return FALSE;

  return _bamf_dbus_item_view_get_running (self->priv->proxy);
}

/**
 * bamf_view_is_urgent:
 * @view: a #BamfView
 *
 * Determines if the view is currently requiring attention. Useful for a running window indicator.
 */
gboolean
bamf_view_is_urgent (BamfView *self)
{
  g_return_val_if_fail (BAMF_IS_VIEW (self), FALSE);

  if (BAMF_VIEW_GET_CLASS (self)->is_urgent)
    return BAMF_VIEW_GET_CLASS (self)->is_urgent (self);

  if (!_bamf_view_remote_ready (self))
    return FALSE;

  return _bamf_dbus_item_view_get_urgent (self->priv->proxy);
}

void
_bamf_view_set_cached_name (BamfView *view, const char *name)
{
  g_return_if_fail (BAMF_IS_VIEW (view));

  if (!name || g_strcmp0 (name, view->priv->cached_name) == 0)
    return;

  g_free (view->priv->cached_name);
  view->priv->cached_name = NULL;

  if (name && name[0] != '\0')
    {
      view->priv->cached_name = g_strdup (name);
    }
}

void
_bamf_view_set_cached_icon (BamfView *view, const char *icon)
{
  g_return_if_fail (BAMF_IS_VIEW (view));

  if (!icon || g_strcmp0 (icon, view->priv->cached_icon) == 0)
    return;

  g_free (view->priv->cached_icon);
  view->priv->cached_icon = NULL;

  if (icon && icon[0] != '\0')
    {
      view->priv->cached_icon = g_strdup (icon);
    }
}

gboolean
bamf_view_is_sticky (BamfView *view)
{
  g_return_val_if_fail (BAMF_IS_VIEW (view), FALSE);

  return view->priv->sticky;
}

void
bamf_view_set_sticky (BamfView *view, gboolean sticky)
{
  g_return_if_fail (BAMF_IS_VIEW (view));

  if (view->priv->sticky == sticky)
    return;

  view->priv->sticky = sticky;

  if (sticky)
    {
      g_object_ref_sink (view);
    }
  else
    {
      g_object_unref (view);
    }

  if (BAMF_VIEW_GET_CLASS (view)->set_sticky)
    return BAMF_VIEW_GET_CLASS (view)->set_sticky (view, sticky);
}

/**
 * bamf_view_get_icon:
 * @view: a #BamfView
 *
 * Gets the icon of a view. This icon is used to visually represent the view.
 */
gchar *
bamf_view_get_icon (BamfView *self)
{
  BamfViewPrivate *priv;

  g_return_val_if_fail (BAMF_IS_VIEW (self), NULL);
  priv = self->priv;

  if (BAMF_VIEW_GET_CLASS (self)->get_icon)
    return BAMF_VIEW_GET_CLASS (self)->get_icon (self);

  if (!_bamf_view_remote_ready (self))
    return g_strdup (priv->cached_icon);

  return _bamf_dbus_item_view_dup_icon (priv->proxy);
}

/**
 * bamf_view_get_name:
 * @view: a #BamfView
 *
 * Gets the name of a view. This name is a short name best used to represent the view with text.
 */
gchar *
bamf_view_get_name (BamfView *self)
{
  BamfViewPrivate *priv;

  g_return_val_if_fail (BAMF_IS_VIEW (self), NULL);
  priv = self->priv;

  if (BAMF_VIEW_GET_CLASS (self)->get_name)
    return BAMF_VIEW_GET_CLASS (self)->get_name (self);

  if (!_bamf_view_remote_ready (self))
    return g_strdup (priv->cached_name);

  return _bamf_dbus_item_view_dup_name (priv->proxy);
}

gboolean
_bamf_view_remote_ready (BamfView *self)
{
  if (BAMF_IS_VIEW (self) && G_IS_DBUS_PROXY (self->priv->proxy))
    return !self->priv->is_closed;

  return FALSE;
}

/**
 * bamf_view_get_view_type:
 * @view: a #BamfView
 *
 * The view type of a window is a short string used to represent all views of the same class. These
 * descriptions should not be used to do casting as they are not considered stable.
 *
 * Virtual: view_type
 */
const gchar *
bamf_view_get_view_type (BamfView *self)
{
  BamfViewPrivate *priv;
  char *type = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_VIEW (self), NULL);
  priv = self->priv;

  if (BAMF_VIEW_GET_CLASS (self)->view_type)
    return BAMF_VIEW_GET_CLASS (self)->view_type (self);

  if (priv->type)
    return priv->type;

  if (!_bamf_view_remote_ready (self))
    return NULL;

  if (!_bamf_dbus_item_view_call_view_type_sync (priv->proxy, &type, CANCELLABLE (self), &error))
    {
      const gchar *path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (priv->proxy));
      g_warning ("Failed to fetch view type at %s: %s", path, error ? error->message : "");
      g_error_free (error);
      return NULL;
    }

  priv->type = type;
  return type;
}

BamfClickBehavior
bamf_view_get_click_suggestion (BamfView *self)
{
  g_return_val_if_fail (BAMF_IS_VIEW (self), BAMF_CLICK_BEHAVIOR_NONE);

  if (BAMF_VIEW_GET_CLASS (self)->click_behavior)
    return BAMF_VIEW_GET_CLASS (self)->click_behavior (self);

  return BAMF_CLICK_BEHAVIOR_NONE;
}

static void
bamf_view_child_xid_changed (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  BamfView *self;

  self = (BamfView *)user_data;

  g_signal_emit (G_OBJECT (self), view_signals[CHILD_MOVED], 0, BAMF_VIEW (object));
  g_signal_emit (G_OBJECT (self), view_signals[VISIBLE_CHANGED], 0);
}

static void
bamf_view_on_child_added (BamfDBusItemView *proxy, const char *path, BamfView *self)
{
  BamfView *view;
  BamfViewPrivate *priv;

  view = _bamf_factory_view_for_path (_bamf_factory_get_default (), path);
  priv = self->priv;

  g_return_if_fail (BAMF_IS_VIEW (view));

  if (BAMF_IS_TAB (view))
    {
      g_signal_connect (view, "notify::xid",
                        G_CALLBACK (bamf_view_child_xid_changed), self);
    }

  if (!g_list_find (priv->cached_children, view))
    {
      g_object_ref (view);
      priv->cached_children = g_list_prepend (priv->cached_children, view);
    }

  g_signal_emit (G_OBJECT (self), view_signals[CHILD_ADDED], 0, view);
}

static void
bamf_view_on_child_removed (BamfDBusItemView *proxy, char *path, BamfView *self)
{
  BamfView *view;
  BamfViewPrivate *priv;

  view = _bamf_factory_view_for_path (_bamf_factory_get_default (), path);
  priv = self->priv;

  g_return_if_fail (BAMF_IS_VIEW (view));

  if (BAMF_IS_TAB (view))
    {
      g_signal_handlers_disconnect_by_func (view, bamf_view_child_xid_changed, self);
    }

  if (priv->cached_children)
    {
      GList *l = g_list_find (priv->cached_children, view);

      if (l)
        {
          priv->cached_children = g_list_delete_link (priv->cached_children, l);
          g_object_unref (view);
        }
    }

  g_signal_emit (G_OBJECT (self), view_signals[CHILD_REMOVED], 0, view);
}

static void
bamf_view_on_name_owner_changed (BamfDBusItemView *proxy, GParamSpec *param, BamfView *self)
{
  /* This is called when the bamfdaemon is killed / started */
  gchar *name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (proxy));

  if (!name_owner)
    {
      if (self->priv->cached_children)
        {
          g_list_free_full (self->priv->cached_children, g_object_unref);
          self->priv->reload_children = TRUE;
          self->priv->cached_children = NULL;
        }

      if (self->priv->cached_name)
        {
          const char *cached_name = self->priv->cached_name;
          g_signal_emit (G_OBJECT (self), view_signals[NAME_CHANGED], 0, NULL, cached_name);
        }

      if (self->priv->cached_icon)
        {
          const char *cached_icon = self->priv->cached_icon;
          g_signal_emit (G_OBJECT (self), view_signals[ICON_CHANGED], 0, cached_icon);
        }

      _bamf_view_set_closed (self, TRUE);
      g_signal_emit (G_OBJECT (self), view_signals[CLOSED], 0);
    }

  g_free (name_owner);
}

static void
bamf_view_on_active_changed (BamfDBusItemView *proxy, GParamSpec *param, BamfView *self)
{
  gboolean active = _bamf_dbus_item_view_get_active (proxy);
  g_signal_emit (G_OBJECT (self), view_signals[ACTIVE_CHANGED], 0, active);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
}

static void
bamf_view_on_name_changed (BamfDBusItemView *proxy, GParamSpec *param, BamfView *self)
{
  const char *new_name = _bamf_dbus_item_view_get_name (proxy);
  const char *old_name = self->priv->cached_name;
  g_signal_emit (self, view_signals[NAME_CHANGED], 0, old_name, new_name);
  _bamf_view_set_cached_name (self, new_name);
}

static void
bamf_view_on_icon_changed (BamfDBusItemView *proxy, GParamSpec *param, BamfView *self)
{
  const char *icon = _bamf_dbus_item_view_get_icon (proxy);
  g_signal_emit (self, view_signals[ICON_CHANGED], 0, icon);
  _bamf_view_set_cached_icon (self, icon);
}

static void
bamf_view_on_running_changed (BamfDBusItemView *proxy, GParamSpec *param, BamfView *self)
{
  gboolean running = _bamf_dbus_item_view_get_running (proxy);
  g_signal_emit (G_OBJECT (self), view_signals[RUNNING_CHANGED], 0, running);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUNNING]);
}

static void
bamf_view_on_urgent_changed (BamfDBusItemView *proxy, GParamSpec *param, BamfView *self)
{
  gboolean urgent = _bamf_dbus_item_view_get_urgent (proxy);
  g_signal_emit (G_OBJECT (self), view_signals[URGENT_CHANGED], 0, urgent);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URGENT]);
}

static void
bamf_view_on_user_visible_changed (BamfDBusItemView *proxy, GParamSpec *param, BamfView *self)
{
  gboolean user_visible = _bamf_dbus_item_view_get_user_visible (proxy);
  g_signal_emit (G_OBJECT (self), view_signals[VISIBLE_CHANGED], 0, user_visible);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USER_VISIBLE]);
}

GCancellable *
_bamf_view_get_cancellable (BamfView *view)
{
  g_return_val_if_fail (BAMF_IS_VIEW (view), NULL);

  return view->priv->cancellable;
}

void
_bamf_view_set_closed (BamfView *self, gboolean closed)
{
  BamfViewPrivate *priv;
  g_return_if_fail (BAMF_IS_VIEW (self));

  priv = self->priv;

  if (priv->is_closed != closed)
    {
      priv->is_closed = closed;

      if (closed)
        {
          g_cancellable_cancel (priv->cancellable);
          g_list_free_full (priv->cached_children, g_object_unref);
          priv->cached_children = NULL;
        }
      else
        {
          g_cancellable_reset (priv->cancellable);
        }
    }
}

static void
bamf_view_on_closed (BamfDBusItemView *proxy, BamfView *self)
{
  _bamf_view_set_closed (self, TRUE);
  g_signal_emit (G_OBJECT (self), view_signals[CLOSED], 0);
}

static void
bamf_view_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bamf_view_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  BamfView *self;

  self = BAMF_VIEW (object);

  switch (property_id)
    {
      case PROP_PATH:
        g_value_set_string (value, bamf_view_is_closed (self) ? NULL : _bamf_view_get_path (self));
        break;

      case PROP_ACTIVE:
        g_value_set_boolean (value, bamf_view_is_active (self));
        break;

      case PROP_RUNNING:
        g_value_set_boolean (value, bamf_view_is_running (self));
        break;

      case PROP_URGENT:
        g_value_set_boolean (value, bamf_view_is_urgent (self));
        break;

      case PROP_USER_VISIBLE:
        g_value_set_boolean (value, bamf_view_is_user_visible (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bamf_view_unset_proxy (BamfView *self)
{
  BamfViewPrivate *priv;

  g_return_if_fail (BAMF_IS_VIEW (self));
  priv = self->priv;

  if (!priv->proxy)
    return;

  g_signal_handlers_disconnect_by_data (priv->proxy, self);

  g_object_unref (priv->proxy);
  priv->proxy = NULL;
}

static void
bamf_view_dispose (GObject *object)
{
  BamfView *view;
  BamfViewPrivate *priv;

  view = BAMF_VIEW (object);

  priv = view->priv;

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }

  if (priv->type)
    {
      g_free (priv->type);
      priv->type = NULL;
    }

  if (priv->cached_icon)
    {
      g_free (priv->cached_icon);
      priv->cached_icon = NULL;
    }

  if (priv->cached_name)
    {
      g_free (priv->cached_name);
      priv->cached_name = NULL;
    }

  if (priv->cached_children)
    {
      g_list_free_full (priv->cached_children, g_object_unref);
      priv->cached_children = NULL;
    }

  bamf_view_unset_proxy (view);

  G_OBJECT_CLASS (bamf_view_parent_class)->dispose (object);
}

const char *
_bamf_view_get_path (BamfView *view)
{
  g_return_val_if_fail (BAMF_IS_VIEW (view), NULL);

  if (G_IS_DBUS_PROXY (view->priv->proxy))
    return g_dbus_proxy_get_object_path (G_DBUS_PROXY (view->priv->proxy));

  return NULL;
}

void
_bamf_view_reset_flags (BamfView *view)
{
  g_return_if_fail (BAMF_IS_VIEW (view));

  /* Notifying proxy properties makes the view to emit proper signals */
  g_object_notify (G_OBJECT (view->priv->proxy), "user-visible");
  g_object_notify (G_OBJECT (view->priv->proxy), "active");
  g_object_notify (G_OBJECT (view->priv->proxy), "running");
  g_object_notify (G_OBJECT (view->priv->proxy), "urgent");
  g_object_notify (G_OBJECT (view->priv->proxy), "name");
}

void
_bamf_view_set_path (BamfView *view, const char *path)
{
  BamfViewPrivate *priv;
  GError *error = NULL;

  g_return_if_fail (BAMF_IS_VIEW (view));
  g_return_if_fail (path);

  _bamf_view_set_closed (view, FALSE);

  if (g_strcmp0 (_bamf_view_get_path (view), path) == 0)
    {
      // The proxy path has not been changed, no need to unset and re-set it again
      _bamf_view_reset_flags (view);
      return;
    }

  bamf_view_unset_proxy (view);

  priv = view->priv;
  priv->reload_children = TRUE;

  priv->proxy = _bamf_dbus_item_view_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                             G_DBUS_PROXY_FLAGS_NONE,
                                                             BAMF_DBUS_SERVICE_NAME,
                                                             path, CANCELLABLE (view),
                                                             &error);
  if (!G_IS_DBUS_PROXY (priv->proxy))
    {
      g_critical ("Unable to get %s view: %s", BAMF_DBUS_SERVICE_NAME, error ? error ? error->message : "" : "");
      g_error_free (error);
      return;
    }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (priv->proxy), BAMF_DBUS_DEFAULT_TIMEOUT);
  g_object_notify_by_pspec (G_OBJECT (view), properties[PROP_PATH]);

  g_signal_connect (priv->proxy, "notify::g-name-owner",
                    G_CALLBACK (bamf_view_on_name_owner_changed), view);

  g_signal_connect (priv->proxy, "notify::active",
                    G_CALLBACK (bamf_view_on_active_changed), view);

  g_signal_connect (priv->proxy, "notify::running",
                    G_CALLBACK (bamf_view_on_running_changed), view);

  g_signal_connect (priv->proxy, "notify::urgent",
                    G_CALLBACK (bamf_view_on_urgent_changed), view);

  g_signal_connect (priv->proxy, "notify::user-visible",
                    G_CALLBACK (bamf_view_on_user_visible_changed), view);

  g_signal_connect (priv->proxy, "notify::name",
                    G_CALLBACK (bamf_view_on_name_changed), view);

  g_signal_connect (priv->proxy, "notify::icon",
                    G_CALLBACK (bamf_view_on_icon_changed), view);

  g_signal_connect (priv->proxy, "child-added",
                    G_CALLBACK (bamf_view_on_child_added), view);

  g_signal_connect (priv->proxy, "child-removed",
                    G_CALLBACK (bamf_view_on_child_removed), view);

  g_signal_connect (priv->proxy, "closed",
                    G_CALLBACK (bamf_view_on_closed), view);

  _bamf_view_reset_flags (view);

  if (BAMF_VIEW_GET_CLASS (view)->set_path)
    BAMF_VIEW_GET_CLASS (view)->set_path (view, path);
}

static void
bamf_view_class_init (BamfViewClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->dispose      = bamf_view_dispose;
  obj_class->get_property = bamf_view_get_property;
  obj_class->set_property = bamf_view_set_property;

  properties[PROP_PATH] = g_param_spec_string ("path", "path", "path", NULL, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_PATH, properties[PROP_PATH]);

  properties[PROP_ACTIVE] = g_param_spec_boolean ("active", "active", "active", FALSE, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_ACTIVE, properties[PROP_ACTIVE]);

  properties[PROP_URGENT] = g_param_spec_boolean ("urgent", "urgent", "urgent", FALSE, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_URGENT, properties[PROP_URGENT]);

  properties[PROP_RUNNING] = g_param_spec_boolean ("running", "running", "running", FALSE, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_RUNNING, properties[PROP_RUNNING]);

  properties[PROP_USER_VISIBLE] = g_param_spec_boolean ("user-visible", "user-visible", "user-visible", FALSE, G_PARAM_READABLE);
  g_object_class_install_property (obj_class, PROP_USER_VISIBLE, properties[PROP_USER_VISIBLE]);

  g_type_class_add_private (obj_class, sizeof (BamfViewPrivate));

  view_signals [ACTIVE_CHANGED] =
    g_signal_new (BAMF_VIEW_SIGNAL_ACTIVE_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfViewClass, active_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  view_signals [CLOSED] =
    g_signal_new (BAMF_VIEW_SIGNAL_CLOSED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (BamfViewClass, closed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  view_signals [CHILD_ADDED] =
    g_signal_new (BAMF_VIEW_SIGNAL_CHILD_ADDED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfViewClass, child_added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_VIEW);

  view_signals [CHILD_REMOVED] =
    g_signal_new (BAMF_VIEW_SIGNAL_CHILD_REMOVED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfViewClass, child_removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_VIEW);

  view_signals [CHILD_MOVED] =
    g_signal_new (BAMF_VIEW_SIGNAL_CHILD_MOVED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfViewClass, child_moved),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_VIEW);

  view_signals [RUNNING_CHANGED] =
    g_signal_new (BAMF_VIEW_SIGNAL_RUNNING_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfViewClass, running_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  view_signals [URGENT_CHANGED] =
    g_signal_new (BAMF_VIEW_SIGNAL_URGENT_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfViewClass, urgent_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  view_signals [VISIBLE_CHANGED] =
    g_signal_new (BAMF_VIEW_SIGNAL_USER_VISIBLE_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (BamfViewClass, user_visible_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  view_signals [NAME_CHANGED] =
    g_signal_new (BAMF_VIEW_SIGNAL_NAME_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  G_STRUCT_OFFSET (BamfViewClass, name_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  view_signals [ICON_CHANGED] =
    g_signal_new (BAMF_VIEW_SIGNAL_ICON_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  G_STRUCT_OFFSET (BamfViewClass, icon_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);
}

static void
bamf_view_init (BamfView *self)
{
  BamfViewPrivate *priv;

  priv = self->priv = BAMF_VIEW_GET_PRIVATE (self);
  priv->cancellable = g_cancellable_new ();
  priv->is_closed = TRUE;
  priv->reload_children = TRUE;
}
