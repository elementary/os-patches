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
 * SECTION:bamf-application
 * @short_description: The base class for all applications
 *
 * #BamfApplication is the base class that all applications need to derive from.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <libbamf-private/bamf-private.h>
#include "bamf-application.h"
#include "bamf-window.h"
#include "bamf-factory.h"
#include "bamf-application-private.h"
#include "bamf-view-private.h"

#include <gio/gdesktopappinfo.h>
#include <string.h>

G_DEFINE_TYPE (BamfApplication, bamf_application, BAMF_TYPE_VIEW);

#define BAMF_APPLICATION_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BAMF_TYPE_APPLICATION, BamfApplicationPrivate))

enum
{
  DESKTOP_FILE_UPDATED,
  WINDOW_ADDED,
  WINDOW_REMOVED,

  LAST_SIGNAL,
};

static guint application_signals[LAST_SIGNAL] = { 0 };

struct _BamfApplicationPrivate
{
  BamfDBusItemApplication *proxy;
  gchar                   *application_type;
  gchar                   *desktop_file;
  GList                   *cached_xids;
  gchar                  **cached_mimes;
  int                      show_stubs;
};

/**
 * bamf_application_get_supported_mime_types:
 * @application: a #BamfApplication
 *
 * Returns: (transfer full) (array zero-terminated=1): A string array containing the supported mime-types.
 */
gchar **
bamf_application_get_supported_mime_types (BamfApplication *application)
{
  BamfApplicationPrivate *priv;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);
  priv = application->priv;

  if (priv->cached_mimes)
    return g_strdupv (priv->cached_mimes);

  if (!_bamf_view_remote_ready (BAMF_VIEW (application)))
    return NULL;

  if (!_bamf_dbus_item_application_call_supported_mime_types_sync (priv->proxy,
                                                                   &priv->cached_mimes,
                                                                   CANCELLABLE (application),
                                                                   &error))
    {
      priv->cached_mimes = NULL;
      g_warning ("Failed to fetch mimes: %s", error ? error->message : "");
      g_error_free (error);
    }

  return g_strdupv (priv->cached_mimes);
}

/**
 * bamf_application_get_desktop_file:
 * @application: a #BamfApplication
 *
 * Used to fetch the path to the .desktop file associated with the passed application. If
 * none exists, the result is NULL.
 *
 * Returns: A string representing the path to the desktop file.
 */
const gchar *
bamf_application_get_desktop_file (BamfApplication *application)
{
  BamfApplicationPrivate *priv;
  gchar *file;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), FALSE);
  priv = application->priv;

  if (priv->desktop_file)
    return priv->desktop_file;

  if (!_bamf_view_remote_ready (BAMF_VIEW (application)))
    return NULL;

  if (!_bamf_dbus_item_application_call_desktop_file_sync (priv->proxy, &file,
                                                           CANCELLABLE (application),
                                                           &error))
    {
      g_warning ("Failed to fetch path: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  if (file && file[0] == '\0')
    {
      g_free (file);
      file = NULL;
    }

  priv->desktop_file = file;
  return file;
}

/**
 * bamf_application_get_application_menu:
 * @application: a #BamfApplication
 * @name: (out): the bus name
 * @object_path: (out): the object path
 *
 * Used to fetch the bus name and the object path of the remote application menu.
 *
 * Deprecated: 0.5.0
 * Returns: %TRUE if found, %FALSE otherwise.
 */
gboolean
bamf_application_get_application_menu (BamfApplication *application,
                                       gchar **name,
                                       gchar **object_path)
{
  BamfApplicationPrivate *priv;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (name != NULL && object_path != NULL, FALSE);

  priv = application->priv;

  if (!_bamf_view_remote_ready (BAMF_VIEW (application)))
    return FALSE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (!_bamf_dbus_item_application_call_application_menu_sync (priv->proxy, name,
                                                               object_path,
                                                               CANCELLABLE (application),
                                                               &error))
G_GNUC_END_IGNORE_DEPRECATIONS
    {
      *name = NULL;
      *object_path = NULL;

      g_warning ("Failed to fetch application menu path: %s", error ? error->message : "");
      g_error_free (error);

      return FALSE;
    }

  return TRUE;
}

/**
 * bamf_application_get_applicaton_type:
 * @application: a #BamfApplication
 *
 * Used to determine what type of application a .desktop file represents. Current values are:
 *  "system" : A normal application, like firefox or evolution
 *  "web"    : A web application, like facebook or twitter
 *
 * Returns: A string
 */
const gchar *
bamf_application_get_application_type (BamfApplication *application)
{
  BamfApplicationPrivate *priv;
  gchar *type;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), FALSE);
  priv = application->priv;

  if (priv->application_type)
    return priv->application_type;

  if (!_bamf_view_remote_ready (BAMF_VIEW (application)))
    return NULL;

  if (!_bamf_dbus_item_application_call_application_type_sync (priv->proxy, &type,
                                                               CANCELLABLE (application),
                                                               &error))
    {
      g_warning ("Failed to fetch path: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  priv->application_type = type;
  return type;
}

/**
 * bamf_application_get_xids:
 * @application: a #BamfApplication
 *
 * Used to fetch all #BamfWindow's xids associated with the passed #BamfApplication.
 *
 * Returns: (element-type guint32) (transfer full): An array of xids.
 */
GArray *
bamf_application_get_xids (BamfApplication *application)
{
  BamfApplicationPrivate *priv;
  GVariantIter *iter;
  GVariant *xids_variant;
  GArray *xids;
  guint32 xid;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), FALSE);
  priv = application->priv;

  if (!_bamf_view_remote_ready (BAMF_VIEW (application)))
    return NULL;

  if (!_bamf_dbus_item_application_call_xids_sync (priv->proxy, &xids_variant,
                                                   CANCELLABLE (application), &error))
    {
      g_warning ("Failed to fetch xids: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  g_return_val_if_fail (xids_variant, NULL);
  g_return_val_if_fail (g_variant_type_equal (g_variant_get_type (xids_variant),
                                              G_VARIANT_TYPE ("au")), NULL);

  xids = g_array_new (FALSE, TRUE, sizeof (guint32));

  g_variant_get (xids_variant, "au", &iter);
  while (g_variant_iter_loop (iter, "u", &xid))
    {
      g_array_append_val (xids, xid);
    }

  g_variant_iter_free (iter);
  g_variant_unref (xids_variant);

  return xids;
}

/**
 * bamf_application_get_windows:
 * @application: a #BamfApplication
 *
 * Used to fetch all #BamfWindow's associated with the passed #BamfApplication.
 *
 * Returns: (element-type Bamf.Window) (transfer container): A list of #BamfWindow's.
 */
GList *
bamf_application_get_windows (BamfApplication *application)
{
  GList *children, *l;
  GList *windows = NULL;
  BamfView *view;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), NULL);

  children = bamf_view_get_children (BAMF_VIEW (application));

  for (l = children; l; l = l->next)
    {
      view = l->data;

      if (BAMF_IS_WINDOW (view))
        {
          windows = g_list_prepend (windows, view);
        }
    }

  g_list_free (children);
  return windows;
}

/**
 * bamf_application_get_show_menu_stubs:
 * @application: a #BamfApplication
 *
 * Used to discover whether the application wants menu stubs shown.
 *
 * Returns: Whether the stubs should be shown.
 */
gboolean
bamf_application_get_show_menu_stubs (BamfApplication * application)
{
  BamfApplicationPrivate *priv;
  GError *error = NULL;
  gboolean result;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), TRUE);

  priv = application->priv;

  if (!_bamf_view_remote_ready (BAMF_VIEW (application)))
    return TRUE;

  if (priv->show_stubs == -1)
    {
      if (!_bamf_dbus_item_application_call_show_stubs_sync (priv->proxy, &result,
                                                             CANCELLABLE (application),
                                                             &error))
        {
          g_warning ("Failed to fetch show_stubs: %s", error ? error->message : "");
          g_error_free (error);

          return TRUE;
        }

      if (result)
        priv->show_stubs = 1;
      else
        priv->show_stubs = 0;
    }

  return priv->show_stubs;
}

static BamfClickBehavior
bamf_application_get_click_suggestion (BamfView *view)
{
  if (!bamf_view_is_running (view))
    return BAMF_CLICK_BEHAVIOR_OPEN;

  return 0;
}

/**
 * bamf_application_get_focusable_child:
 * @application: a #BamfApplication
 *
 * Returns: (transfer none): The focusable child for this application.
 */
BamfView *
bamf_application_get_focusable_child (BamfApplication *application)
{
  BamfApplicationPrivate *priv;
  BamfView *ret;
  gchar *path;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_APPLICATION (application), FALSE);
  priv = application->priv;

  if (!_bamf_view_remote_ready (BAMF_VIEW (application)))
    return NULL;

  if (!_bamf_dbus_item_application_call_focusable_child_sync (priv->proxy, &path,
                                                              CANCELLABLE (application),
                                                              &error))
    {
      g_warning ("Failed to fetch focusable child: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  ret = _bamf_factory_view_for_path (_bamf_factory_get_default (), path);
  g_free (path);

  return ret;
}

static void
bamf_application_on_desktop_file_updated (BamfDBusItemApplication *proxy, const char *desktop_file, BamfApplication *self)
{
  g_free (self->priv->desktop_file);
  self->priv->desktop_file = g_strdup (desktop_file);

  g_signal_emit (self, application_signals[DESKTOP_FILE_UPDATED], 0, desktop_file);
}

static void
bamf_application_on_window_added (BamfDBusItemApplication *proxy, const char *path, BamfApplication *self)
{
  BamfView *view;
  BamfFactory *factory;

  g_return_if_fail (BAMF_IS_APPLICATION (self));

  factory = _bamf_factory_get_default ();
  view = _bamf_factory_view_for_path_type (factory, path, BAMF_FACTORY_WINDOW);

  if (BAMF_IS_WINDOW (view))
    {
      guint32 xid = bamf_window_get_xid (BAMF_WINDOW (view));

      if (!g_list_find (self->priv->cached_xids, GUINT_TO_POINTER (xid)))
        {
          self->priv->cached_xids = g_list_prepend (self->priv->cached_xids, GUINT_TO_POINTER (xid));
        }

      g_signal_emit (G_OBJECT (self), application_signals[WINDOW_ADDED], 0, view);
    }
}

static void
bamf_application_on_window_removed (BamfDBusItemApplication *proxy, const char *path, BamfApplication *self)
{
  BamfView *view;
  BamfFactory *factory;

  g_return_if_fail (BAMF_IS_APPLICATION (self));

  factory = _bamf_factory_get_default ();
  view = _bamf_factory_view_for_path_type (factory, path, BAMF_FACTORY_WINDOW);

  if (BAMF_IS_WINDOW (view))
    {
      guint32 xid = bamf_window_get_xid (BAMF_WINDOW (view));
      self->priv->cached_xids = g_list_remove (self->priv->cached_xids, GUINT_TO_POINTER (xid));

      g_signal_emit (G_OBJECT (self), application_signals[WINDOW_REMOVED], 0, view);
    }
}

GList *
_bamf_application_get_cached_xids (BamfApplication *self)
{
  g_return_val_if_fail (BAMF_IS_APPLICATION (self), NULL);

  return self->priv->cached_xids;
}

static void
bamf_application_on_supported_mime_types_changed (BamfDBusItemApplication *proxy, const gchar *const *mimes, BamfApplication *self)
{
  if (self->priv->cached_mimes)
    g_strfreev (self->priv->cached_mimes);

  self->priv->cached_mimes = g_strdupv ((gchar**)mimes);
}

static void
bamf_application_unset_proxy (BamfApplication* self)
{
  BamfApplicationPrivate *priv;

  g_return_if_fail (BAMF_IS_APPLICATION (self));
  priv = self->priv;

  if (G_IS_DBUS_PROXY (priv->proxy))
    {
      g_signal_handlers_disconnect_by_data (priv->proxy, self);
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }
}

static void
bamf_application_dispose (GObject *object)
{
  BamfApplication *self;
  BamfApplicationPrivate *priv;

  self = BAMF_APPLICATION (object);
  priv = self->priv;

  if (priv->application_type)
    {
      g_free (priv->application_type);
      priv->application_type = NULL;
    }

  if (priv->desktop_file)
    {
      g_free (priv->desktop_file);
      priv->desktop_file = NULL;
    }

  if (priv->cached_xids)
    {
      g_list_free (priv->cached_xids);
      priv->cached_xids = NULL;
    }

  if (priv->cached_mimes)
    {
      g_strfreev (priv->cached_mimes);
      priv->cached_mimes = NULL;
    }

  bamf_application_unset_proxy (self);

  if (G_OBJECT_CLASS (bamf_application_parent_class)->dispose)
    G_OBJECT_CLASS (bamf_application_parent_class)->dispose (object);
}

static void
bamf_application_set_path (BamfView *view, const char *path)
{
  BamfApplication *self;
  BamfApplicationPrivate *priv;
  GError *error = NULL;

  self = BAMF_APPLICATION (view);
  priv = self->priv;

  bamf_application_unset_proxy (self);
  priv->proxy = _bamf_dbus_item_application_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                                    BAMF_DBUS_SERVICE_NAME,
                                                                    path, CANCELLABLE (view),
                                                                    &error);

  if (!G_IS_DBUS_PROXY (priv->proxy))
    {
      g_critical ("Unable to get %s application: %s", BAMF_DBUS_SERVICE_NAME, error ? error->message : "");
      g_clear_error (&error);
      return;
    }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (priv->proxy), BAMF_DBUS_DEFAULT_TIMEOUT);

  g_signal_connect (priv->proxy, "desktop-file-updated",
                    G_CALLBACK (bamf_application_on_desktop_file_updated), view);

  g_signal_connect (priv->proxy, "window-added",
                    G_CALLBACK (bamf_application_on_window_added), view);

  g_signal_connect (priv->proxy, "window-removed",
                    G_CALLBACK (bamf_application_on_window_removed), view);

  g_signal_connect (priv->proxy, "supported-mime-types-changed",
                    G_CALLBACK (bamf_application_on_supported_mime_types_changed), view);

  GList *children, *l;
  children = bamf_view_get_children (view);

  if (priv->cached_xids)
    {
      g_list_free (priv->cached_xids);
      priv->cached_xids = NULL;
    }

  for (l = children; l; l = l->next)
    {
      if (!BAMF_IS_WINDOW (l->data))
        continue;

      guint32 xid = bamf_window_get_xid (BAMF_WINDOW (l->data));
      priv->cached_xids = g_list_prepend (priv->cached_xids, GUINT_TO_POINTER (xid));
    }

  g_list_free (children);
}

static void
bamf_application_set_sticky (BamfView *view, gboolean sticky)
{
  BamfApplication *self = BAMF_APPLICATION (view);

  if (sticky)
    {
      bamf_application_get_desktop_file (self);
      bamf_application_get_application_type (self);

      /* When setting the application sticky, we need to cache the relevant values */
      if (!self->priv->cached_mimes)
        {
          gchar **tmp_mimes = bamf_application_get_supported_mime_types (self);
          g_strfreev (tmp_mimes);
        }

      gchar *tmp;
      tmp = bamf_view_get_icon (view);
      g_free (tmp);

      tmp = bamf_view_get_name (view);
      g_free (tmp);
    }
}

static void
bamf_application_load_data_from_file (BamfApplication *self, GKeyFile * keyfile)
{
  GDesktopAppInfo *desktop_info;
  GIcon *gicon;
  char *fullname;
  char *name;
  char *icon;

  g_return_if_fail (keyfile);

  desktop_info = g_desktop_app_info_new_from_keyfile (keyfile);
  g_return_if_fail (G_IS_DESKTOP_APP_INFO (desktop_info));

  name = g_strdup (g_app_info_get_name (G_APP_INFO (desktop_info)));

  /* Grab the better name if its available */
  fullname = g_key_file_get_locale_string (keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                           G_KEY_FILE_DESKTOP_KEY_FULLNAME, NULL, NULL);

  if (fullname && fullname[0] == '\0')
    {
      g_free (fullname);
      fullname = NULL;
    }

  if (fullname)
    {
      g_free (name);
      name = fullname;
    }

  _bamf_view_set_cached_name (BAMF_VIEW (self), name);

  gicon = g_app_info_get_icon (G_APP_INFO (desktop_info));
  icon = gicon ? g_icon_to_string (gicon) : NULL;

  if (!icon)
    icon = g_strdup (BAMF_APPLICATION_DEFAULT_ICON);

  _bamf_view_set_cached_icon (BAMF_VIEW (self), icon);

  self->priv->cached_mimes = g_key_file_get_string_list (keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                                         G_KEY_FILE_DESKTOP_KEY_MIME_TYPE, NULL, NULL);

  self->priv->application_type = g_strdup ("system");

  g_free (icon);
  g_free (name);
  g_key_file_free (keyfile);
  g_object_unref (desktop_info);
}

static void
bamf_application_class_init (BamfApplicationClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  BamfViewClass *view_class = BAMF_VIEW_CLASS (klass);

  obj_class->dispose     = bamf_application_dispose;
  view_class->set_path   = bamf_application_set_path;
  view_class->set_sticky = bamf_application_set_sticky;
  view_class->click_behavior = bamf_application_get_click_suggestion;

  g_type_class_add_private (obj_class, sizeof (BamfApplicationPrivate));

  application_signals [DESKTOP_FILE_UPDATED] =
    g_signal_new (BAMF_APPLICATION_SIGNAL_DESKTOP_FILE_UPDATED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  application_signals [WINDOW_ADDED] =
    g_signal_new (BAMF_APPLICATION_SIGNAL_WINDOW_ADDED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_WINDOW);

  application_signals [WINDOW_REMOVED] =
    g_signal_new (BAMF_APPLICATION_SIGNAL_WINDOW_REMOVED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_WINDOW);
}


static void
bamf_application_init (BamfApplication *self)
{
  BamfApplicationPrivate *priv;

  priv = self->priv = BAMF_APPLICATION_GET_PRIVATE (self);
  priv->show_stubs = -1;
}

BamfApplication *
bamf_application_new (const char * path)
{
  BamfApplication *self;
  self = g_object_new (BAMF_TYPE_APPLICATION, NULL);

  _bamf_view_set_path (BAMF_VIEW (self), path);

  return self;
}

BamfApplication *
bamf_application_new_favorite (const char * favorite_path)
{
  BamfApplication *self;
  GKeyFile        *desktop_keyfile;
  gchar           *type;
  gboolean         supported = FALSE;

  g_return_val_if_fail (favorite_path, NULL);

  // check that we support this kind of desktop file
  desktop_keyfile = g_key_file_new ();

  if (g_key_file_load_from_file (desktop_keyfile, favorite_path, G_KEY_FILE_NONE, NULL))
    {
      type = g_key_file_get_string (desktop_keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                    G_KEY_FILE_DESKTOP_KEY_TYPE, NULL);

      if (g_strcmp0 (type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) == 0)
        supported = TRUE;

      g_free (type);
    }

  if (!supported)
    {
      g_key_file_free (desktop_keyfile);
      return NULL;
    }

  self = g_object_new (BAMF_TYPE_APPLICATION, NULL);

  self->priv->desktop_file = g_strdup (favorite_path);
  bamf_application_load_data_from_file (self, desktop_keyfile);

  return self;
}
