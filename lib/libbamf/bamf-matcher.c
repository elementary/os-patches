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
 * SECTION:bamf-matcher
 * @short_description: The base class for all matchers
 *
 * #BamfMatcher is the base class that all matchers need to derive from.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <libbamf-private/bamf-private.h>
#include "bamf-matcher.h"
#include "bamf-tab.h"
#include "bamf-view.h"
#include "bamf-view-private.h"
#include "bamf-factory.h"

G_DEFINE_TYPE (BamfMatcher, bamf_matcher, G_TYPE_OBJECT);

#define BAMF_MATCHER_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BAMF_TYPE_MATCHER, BamfMatcherPrivate))

enum
{
  VIEW_OPENED,
  VIEW_CLOSED,
  ACTIVE_APPLICATION_CHANGED,
  ACTIVE_WINDOW_CHANGED,
  STACKING_ORDER_CHANGED,
  LAST_SIGNAL,
};

static guint matcher_signals[LAST_SIGNAL] = { 0 };

struct _BamfMatcherPrivate
{
  BamfDBusMatcher *proxy;
  GCancellable    *cancellable;
};

static BamfMatcher * default_matcher = NULL;

static void bamf_matcher_dispose (GObject *object);

static void
bamf_matcher_finalize (GObject *object)
{
  default_matcher = NULL;

  g_object_unref (_bamf_factory_get_default ());

  G_OBJECT_CLASS (bamf_matcher_parent_class)->finalize (object);
}

static void
bamf_matcher_class_init (BamfMatcherClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (obj_class, sizeof (BamfMatcherPrivate));
  obj_class->dispose = bamf_matcher_dispose;
  obj_class->finalize = bamf_matcher_finalize;

  matcher_signals [VIEW_OPENED] =
    g_signal_new (BAMF_MATCHER_SIGNAL_VIEW_OPENED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_VIEW);

  matcher_signals [VIEW_CLOSED] =
    g_signal_new (BAMF_MATCHER_SIGNAL_VIEW_CLOSED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  BAMF_TYPE_VIEW);

  matcher_signals [ACTIVE_APPLICATION_CHANGED] =
    g_signal_new (BAMF_MATCHER_SIGNAL_ACTIVE_APPLICATION_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  BAMF_TYPE_APPLICATION, BAMF_TYPE_APPLICATION);

  matcher_signals [ACTIVE_WINDOW_CHANGED] =
    g_signal_new (BAMF_MATCHER_SIGNAL_ACTIVE_WINDOW_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  BAMF_TYPE_WINDOW, BAMF_TYPE_WINDOW);

  matcher_signals [STACKING_ORDER_CHANGED] =
    g_signal_new (BAMF_MATCHER_SIGNAL_STACKING_ORDER_CHANGED,
                  G_OBJECT_CLASS_TYPE (klass),
                  0,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}


static void
bamf_matcher_on_view_opened (BamfDBusMatcher *proxy,
                             const char *path,
                             const char *type,
                             BamfMatcher *matcher)
{
  BamfView *view;
  BamfFactory *factory = _bamf_factory_get_default ();

  view = _bamf_factory_view_for_path_type_str (factory, path, type);

  if (!BAMF_IS_VIEW (view))
    return;

  /* We manually set the view as not closed, to avoid issues like bug #925421 */
  _bamf_view_set_closed (view, FALSE);
  g_signal_emit (matcher, matcher_signals[VIEW_OPENED], 0, view);
}

static void
bamf_matcher_on_view_closed (BamfDBusMatcher *proxy,
                             const char *path,
                             const char *type,
                             BamfMatcher *matcher)
{
  BamfView *view;
  BamfFactory *factory = _bamf_factory_get_default ();

  view = _bamf_factory_view_for_path_type_str (factory, path, type);

  if (!BAMF_IS_VIEW (view))
    return;

  /* We manually set the view as closed, to avoid issues like bug #925421 */
  _bamf_view_set_closed (view, TRUE);
  g_signal_emit (matcher, matcher_signals[VIEW_CLOSED], 0, view);
}

static void
bamf_matcher_on_active_application_changed (BamfDBusMatcher *proxy,
                                            const char *old_path,
                                            const char *new_path,
                                            BamfMatcher *matcher)
{
  BamfView *old_view;
  BamfView *new_view;

  BamfFactory *factory = _bamf_factory_get_default ();
  old_view = _bamf_factory_view_for_path_type (factory, old_path, BAMF_FACTORY_APPLICATION);
  new_view = _bamf_factory_view_for_path_type (factory, new_path, BAMF_FACTORY_APPLICATION);

  g_signal_emit (matcher, matcher_signals[ACTIVE_APPLICATION_CHANGED], 0, old_view, new_view);
}

static void
bamf_matcher_on_active_window_changed (BamfDBusMatcher *proxy,
                                       const char *old_path,
                                       const char *new_path,
                                       BamfMatcher *matcher)
{
  BamfView *old_view;
  BamfView *new_view;

  BamfFactory *factory = _bamf_factory_get_default ();
  old_view = _bamf_factory_view_for_path_type (factory, old_path, BAMF_FACTORY_WINDOW);
  new_view = _bamf_factory_view_for_path_type (factory, new_path, BAMF_FACTORY_WINDOW);

  g_signal_emit (matcher, matcher_signals[ACTIVE_WINDOW_CHANGED], 0, old_view, new_view);
}

static void
bamf_matcher_on_stacking_order_changed (BamfDBusMatcher *proxy, BamfMatcher *matcher)
{
  g_signal_emit (matcher, matcher_signals[STACKING_ORDER_CHANGED], 0);
}

static void
bamf_matcher_init (BamfMatcher *self)
{
  BamfMatcherPrivate *priv;
  GError *error = NULL;

  priv = self->priv = BAMF_MATCHER_GET_PRIVATE (self);
  priv->cancellable = g_cancellable_new ();
  priv->proxy = _bamf_dbus_matcher_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                           G_DBUS_PROXY_FLAGS_NONE,
                                                           BAMF_DBUS_SERVICE_NAME,
                                                           BAMF_DBUS_MATCHER_PATH,
                                                           priv->cancellable, &error);

  if (error)
    {
      g_error ("Unable to get %s matcher: %s", BAMF_DBUS_SERVICE_NAME, error ? error->message : "");
      g_error_free (error);
    }

  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (priv->proxy), BAMF_DBUS_DEFAULT_TIMEOUT);

  g_signal_connect (priv->proxy, "view-opened",
                    G_CALLBACK (bamf_matcher_on_view_opened), self);

  g_signal_connect (priv->proxy, "view-closed",
                    G_CALLBACK (bamf_matcher_on_view_closed), self);

  g_signal_connect (priv->proxy, "active-application-changed",
                    G_CALLBACK (bamf_matcher_on_active_application_changed), self);

  g_signal_connect (priv->proxy, "active-window-changed",
                    G_CALLBACK (bamf_matcher_on_active_window_changed), self);

  g_signal_connect (priv->proxy, "stacking-order-changed",
                    G_CALLBACK (bamf_matcher_on_stacking_order_changed), self);
}

static void
bamf_matcher_dispose (GObject *object)
{
  BamfMatcher *self = BAMF_MATCHER (object);

  if (G_IS_DBUS_PROXY (self->priv->proxy))
    {
      g_signal_handlers_disconnect_by_data (self->priv->proxy, self);
      g_object_unref (self->priv->proxy);
      self->priv->proxy = NULL;
    }

  if (G_IS_CANCELLABLE (self->priv->cancellable))
    {
      g_cancellable_cancel (self->priv->cancellable);
      g_object_unref (self->priv->cancellable);
    }

  G_OBJECT_CLASS (bamf_matcher_parent_class)->dispose (object);
}

/*
 * Public Methods
 */

/**
 * bamf_matcher_get_default:
 *
 * Returns the default matcher. This matcher is owned by bamf and shared between other callers.
 *
 * Returns: (transfer full): A new #BamfMatcher
 */
BamfMatcher *
bamf_matcher_get_default (void)
{
  if (BAMF_IS_MATCHER (default_matcher))
    return g_object_ref (default_matcher);

  return (default_matcher = g_object_new (BAMF_TYPE_MATCHER, NULL));
}

/**
 * bamf_matcher_get_active_application:
 * @matcher: a #BamfMatcher
 *
 * Used to fetch the active #BamfApplication.
 *
 * Returns: (transfer none): The active #BamfApplication.
 */
BamfApplication *
bamf_matcher_get_active_application (BamfMatcher *matcher)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char *app = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_active_application_sync (priv->proxy, &app, priv->cancellable, &error))
    {
      g_warning ("Failed to get active application: %s", error ? error->message : "");
      g_error_free (error);
      return NULL;
    }

  if (app && app[0] == '\0')
    {
      g_free (app);
      return NULL;
    }

  if (!app)
    return NULL;

  BamfFactory *factory = _bamf_factory_get_default ();
  view = _bamf_factory_view_for_path_type (factory, app, BAMF_FACTORY_APPLICATION);
  g_free (app);

  if (!BAMF_IS_APPLICATION (view))
    return NULL;

  return BAMF_APPLICATION (view);
}

/**
 * bamf_matcher_get_active_window:
 * @matcher: a #BamfMatcher
 *
 * Used to fetch the active #BamfWindow.
 *
 * Returns: (transfer none): The active #BamfWindow.
 */
BamfWindow *
bamf_matcher_get_active_window (BamfMatcher *matcher)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char *win = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_active_window_sync (priv->proxy, &win, priv->cancellable, &error))
    {
      g_warning ("Failed to get active window: %s", error ? error->message : "");
      g_error_free (error);
      return NULL;
    }

  if (win && win[0] == '\0')
    {
      g_free (win);
      return NULL;
    }

  if (!win)
    return NULL;

  BamfFactory *factory = _bamf_factory_get_default ();
  view = _bamf_factory_view_for_path_type (factory, win, BAMF_FACTORY_WINDOW);
  g_free (win);

  if (!BAMF_IS_WINDOW (view))
    return NULL;

  return BAMF_WINDOW (view);
}

/**
 * bamf_matcher_get_application_for_window:
 * @matcher: a #BamfMatcher
 * @window: The window to look for
 *
 * Used to fetch the #BamfApplication containing the passed window.
 *
 * Returns: (transfer none): The #BamfApplication representing the xid passed, or NULL if none exists.
 */
BamfApplication *
bamf_matcher_get_application_for_window (BamfMatcher *matcher, BamfWindow *window)
{
  g_return_val_if_fail(BAMF_IS_WINDOW(window), NULL);
  return bamf_matcher_get_application_for_xid (matcher, bamf_window_get_xid(window));
}

/**
 * bamf_matcher_get_application_for_xid:
 * @matcher: a #BamfMatcher
 * @xid: The XID to search for
 *
 * Used to fetch the #BamfApplication containing the passed xid.
 *
 * Returns: (transfer none): The #BamfApplication representing the xid passed, or NULL if none exists.
 */
BamfApplication *
bamf_matcher_get_application_for_xid (BamfMatcher  *matcher, guint32 xid)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char *app = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_application_for_xid_sync (priv->proxy, xid, &app, priv->cancellable, &error))
    {
      g_warning ("Failed to get application for xid %u: %s", xid, error ? error->message : "");
      g_error_free (error);
      return NULL;
    }

  if (app && app[0] == '\0')
    {
      g_free (app);
      return NULL;
    }

  if (!app)
    return NULL;

  BamfFactory *factory = _bamf_factory_get_default ();
  view = _bamf_factory_view_for_path_type (factory, app, BAMF_FACTORY_APPLICATION);
  g_free (app);

  if (!BAMF_IS_APPLICATION (view))
    return NULL;

  return BAMF_APPLICATION (view);
}

gboolean
bamf_matcher_application_is_running (BamfMatcher *matcher, const gchar *app)
{
  BamfMatcherPrivate *priv;
  gboolean running = FALSE;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), FALSE);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_application_is_running_sync (priv->proxy,
                                                            app ? app : "",
                                                            &running,
                                                            priv->cancellable,
                                                            &error))
    {
      g_warning ("Failed to fetch running status: %s", error ? error->message : "");
      g_error_free (error);

      return FALSE;
    }

  return running;
}

/**
 * bamf_matcher_get_applications:
 * @matcher: a #BamfMatcher
 *
 * Used to fetch all #BamfApplication's running or not. Application authors who wish to only
 * see running applications should use bamf_matcher_get_running_applications instead. The reason
 * this method is needed is bamf will occasionally track applications which are not currently
 * running for nefarious purposes.
 *
 * Returns: (element-type Bamf.Application) (transfer container): A list of #BamfApplication's.
 */
GList *
bamf_matcher_get_applications (BamfMatcher *matcher)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char **array = NULL;
  int i, len;
  GList *result = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_application_paths_sync (priv->proxy, &array, priv->cancellable, &error))
    {
      g_warning ("Failed to fetch applications paths: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  if (!array)
    return NULL;

  BamfFactory *factory = _bamf_factory_get_default ();
  len = g_strv_length (array);
  for (i = len-1; i >= 0; --i)
    {
      view = _bamf_factory_view_for_path_type (factory, array[i], BAMF_FACTORY_APPLICATION);

      if (BAMF_IS_APPLICATION (view))
        {
          if (!g_list_find (result, view))
            result = g_list_prepend (result, view);
        }
    }

  g_strfreev (array);
  return result;
}

/**
 * bamf_matcher_get_windows:
 * @matcher: a #BamfMatcher
 *
 * Used to fetch all windows that BAMF knows about.
 *
 * Returns: (element-type Bamf.Window) (transfer container): A list of #BamfWindow's.
 */
GList *
bamf_matcher_get_windows (BamfMatcher *matcher)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char **array = NULL;
  int i, len;
  GList *result = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_window_paths_sync (priv->proxy, &array, priv->cancellable, &error))
    {
      g_warning ("Failed to fetch windows paths: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  if (!array)
    return NULL;

  BamfFactory *factory = _bamf_factory_get_default ();
  len = g_strv_length (array);
  for (i = len-1; i >= 0; --i)
    {
      view = _bamf_factory_view_for_path_type (factory, array[i], BAMF_FACTORY_WINDOW);

      if (BAMF_IS_WINDOW (view))
        result = g_list_prepend (result, view);
    }

  g_strfreev (array);
  return result;
}

/**
 * bamf_matcher_get_window_stack_for_monitor:
 * @matcher: a #BamfMatcher
 * @monitor: the monitor you want the stack from, negative value to get all
 *
 * Used to fetch all windows that BAMF knows about in the requested screen,
 * in stacking bottom-to-top order. If the @monitor is set to a negative value,
 * then it fetches all the available windows in all monitors.
 *
 * Returns: (element-type Bamf.Window) (transfer container): A list of #BamfWindow's.
 */
GList *
bamf_matcher_get_window_stack_for_monitor (BamfMatcher *matcher, gint monitor)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char **array = NULL;
  int i, len;
  GList *result = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_window_stack_for_monitor_sync (priv->proxy, monitor,
                                                              &array, priv->cancellable,
                                                              &error))
    {
      g_warning ("Failed to fetch paths: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  if (!array)
    return NULL;

  BamfFactory *factory = _bamf_factory_get_default ();
  len = g_strv_length (array);
  for (i = len-1; i >= 0; --i)
    {
      view = _bamf_factory_view_for_path_type (factory, array[i], BAMF_FACTORY_WINDOW);

      if (BAMF_IS_WINDOW (view))
        result = g_list_prepend (result, view);
    }

  g_strfreev (array);
  return result;
}


/**
 * bamf_matcher_register_favorites:
 * @matcher: a #BamfMatcher
 * @favorites: (array): an array of strings, each containing an absolute path to a .desktop file
 *
 * Used to effect how bamf performs matching. Desktop files passed to this method will
 * be prefered by bamf to system desktop files.
 */
void
bamf_matcher_register_favorites (BamfMatcher *matcher,
                                 const gchar **favorites)
{
  BamfMatcherPrivate *priv;
  GError *error = NULL;

  g_return_if_fail (BAMF_IS_MATCHER (matcher));
  g_return_if_fail (favorites);

  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_register_favorites_sync (priv->proxy, favorites, priv->cancellable, &error))
    {
      g_warning ("Failed to register favorites: %s", error ? error->message : "");
      g_error_free (error);
    }
}

/**
 * bamf_matcher_get_running_applications:
 * @matcher: a #BamfMatcher
 *
 * Used to fetch all #BamfApplication's which are running.
 *
 * Returns: (element-type Bamf.Application) (transfer container): A list of #BamfApplication's.
 */
GList *
bamf_matcher_get_running_applications (BamfMatcher *matcher)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char **array = NULL;
  int i, len;
  GList *result = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_running_applications_sync (priv->proxy, &array, priv->cancellable, &error))
    {
      g_warning ("Failed to get running applications: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  if (!array)
    return NULL;

  BamfFactory *factory = _bamf_factory_get_default ();
  len = g_strv_length (array);
  for (i = len-1; i >= 0; --i)
    {
      view = _bamf_factory_view_for_path_type (factory, array[i], BAMF_FACTORY_APPLICATION);

      if (BAMF_IS_APPLICATION (view))
        result = g_list_prepend (result, view);
    }

  g_strfreev (array);
  return result;
}

/**
 * bamf_matcher_get_tabs:
 * @matcher: a #BamfMatcher
 *
 * Used to fetch all #BamfView's representing tabs. Currently unused.
 *
 * Returns: (element-type Bamf.Tab) (transfer container): A list of #BamfTab's.
 */
GList *
bamf_matcher_get_tabs (BamfMatcher *matcher)
{
  BamfMatcherPrivate *priv;
  BamfView *view;
  char **array = NULL;
  int i, len;
  GList *result = NULL;
  GError *error = NULL;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_tab_paths_sync (priv->proxy, &array, priv->cancellable, &error))
    {
      g_warning ("Failed to get tabs: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  g_return_val_if_fail (array, NULL);

  BamfFactory *factory = _bamf_factory_get_default ();
  len = g_strv_length (array);
  for (i = len-1; i >= 0; --i)
    {
      view = _bamf_factory_view_for_path_type (factory, array[i], BAMF_FACTORY_TAB);

      if (BAMF_IS_TAB (view))
        result = g_list_prepend (result, view);
    }

  g_strfreev (array);
  return result;
}

/**
 * bamf_matcher_get_xids_for_application:
 * @matcher: a #BamfMatcher
 *
 * Used to fetch all xid's associated with an application. Useful for performing window
 *
 *
 * Returns: (element-type guint32) (transfer full): A list of xids.
 */
GArray *
bamf_matcher_get_xids_for_application (BamfMatcher *matcher,
                                       const gchar *application)
{
  BamfMatcherPrivate *priv;
  GArray *result = NULL;
  GVariant *xids = NULL;
  GVariantIter *iter;
  GError *error = NULL;
  guint32 xid = 0;

  g_return_val_if_fail (BAMF_IS_MATCHER (matcher), NULL);
  priv = matcher->priv;

  if (!_bamf_dbus_matcher_call_xids_for_application_sync (priv->proxy, application,
                                                          &xids, priv->cancellable,
                                                          &error))
    {
      g_warning ("Failed to get xids: %s", error ? error->message : "");
      g_error_free (error);

      return NULL;
    }

  g_return_val_if_fail (xids, NULL);
  g_return_val_if_fail (g_variant_type_equal (g_variant_get_type (xids),
                                              G_VARIANT_TYPE ("au")), NULL);

  result = g_array_new (FALSE, TRUE, sizeof (guint32));

  g_variant_get (xids, "au", &iter);
  while (g_variant_iter_loop (iter, "u", &xid))
    {
      g_array_append_val (result, xid);
    }

  g_variant_iter_free (iter);
  g_variant_unref (xids);

  return result;
}

/**
 * bamf_matcher_get_application_for_desktop_file:
 * @matcher: a #BamfMatcher
 * @desktop_file_path: Path to the desktop file
 * @create_if_not_found: Create a #BamfApplication if one isn't found
 *
 * Returns: (transfer none): A #BamfApplication for given desktop file.
 */
BamfApplication *
bamf_matcher_get_application_for_desktop_file (BamfMatcher *matcher,
                                               const gchar *desktop_file_path,
                                               gboolean create_if_not_found)
{
  BamfApplication *app;
  const gchar ** favs;

  app = _bamf_factory_app_for_file (_bamf_factory_get_default (), desktop_file_path, create_if_not_found);

  if (app)
    {
      favs = g_new0 (const gchar *, 2);
      favs[0] = desktop_file_path;
      bamf_matcher_register_favorites (matcher, favs);
      g_free (favs);
    }

  return app;
}
