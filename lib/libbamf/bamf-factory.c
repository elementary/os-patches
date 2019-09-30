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
 *              Neil Jagdish Patel <neil.patel@canonical.com>
 *              Marco Trevisan (Treviño) <marco.trevisan@canonical.com>
 *
 */
/**
 * SECTION:bamf-factory
 * @short_description: The base class for all factorys
 *
 * #BamfFactory is the base class that all factorys need to derive from.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <libbamf-private/bamf-private.h>
#include "bamf-factory.h"
#include "bamf-application-private.h"
#include "bamf-view-private.h"

G_DEFINE_TYPE (BamfFactory, bamf_factory, G_TYPE_OBJECT);

#define BAMF_FACTORY_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BAMF_TYPE_FACTORY, BamfFactoryPrivate))

struct _BamfFactoryPrivate
{
  GHashTable *open_views;
  GList *allocated_views;
};

static BamfFactory *static_factory = NULL;

static void on_view_weak_unref (BamfFactory *self, BamfView *view_was_here);

static void
bamf_factory_dispose (GObject *object)
{
  GList *l, *next;
  BamfFactory *self = BAMF_FACTORY (object);

  if (self->priv->allocated_views)
    {
      for (l = self->priv->allocated_views, next = NULL; l; l = next)
        {
          g_object_weak_unref (G_OBJECT (l->data), (GWeakNotify) on_view_weak_unref, self);
          next = l->next;
          g_list_free1 (l);
        }

      self->priv->allocated_views = NULL;
    }

  if (self->priv->open_views)
    {
      g_hash_table_remove_all (self->priv->open_views);
      self->priv->open_views = NULL;
    }

  G_OBJECT_CLASS (bamf_factory_parent_class)->dispose (object);
}

static void
bamf_factory_finalize (GObject *object)
{
  BamfFactory *self = BAMF_FACTORY (object);

  if (self->priv->open_views)
    {
      g_hash_table_destroy (self->priv->open_views);
      self->priv->open_views = NULL;
    }

  static_factory = NULL;

  G_OBJECT_CLASS (bamf_factory_parent_class)->finalize (object);
}

static void
bamf_factory_class_init (BamfFactoryClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->dispose = bamf_factory_dispose;
  obj_class->finalize = bamf_factory_finalize;

  g_type_class_add_private (obj_class, sizeof (BamfFactoryPrivate));
}


static void
bamf_factory_init (BamfFactory *self)
{
  self->priv = BAMF_FACTORY_GET_PRIVATE (self);
  self->priv->open_views = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, g_object_unref);
}

static void
on_view_closed (BamfView *view, BamfFactory *self)
{
  const char *path;
  gboolean removed;

  removed = FALSE;
  path = _bamf_view_get_path (view);

  g_signal_handlers_disconnect_by_func (view, on_view_closed, self);

  if (path)
    {
      removed = g_hash_table_remove (self->priv->open_views, path);
    }

  if (G_UNLIKELY (!removed))
    {
      /* Unlikely to happen, but who knows... */
      GHashTableIter iter;
      gpointer value;

      g_hash_table_iter_init (&iter, self->priv->open_views);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          if (value == view)
            {
              g_hash_table_iter_remove (&iter);
              removed = TRUE;
              break;
            }
        }
    }
}

static void
on_view_weak_unref (BamfFactory *self, BamfView *view_was_here)
{
  self->priv->allocated_views = g_list_remove (self->priv->allocated_views, view_was_here);
}

static void
bamf_factory_track_view (BamfFactory *self, BamfView *view)
{
  g_return_if_fail (BAMF_IS_VIEW (view));

  if (g_list_find (self->priv->allocated_views, view))
    return;

  g_object_weak_ref (G_OBJECT (view), (GWeakNotify) on_view_weak_unref, self);
  self->priv->allocated_views = g_list_prepend (self->priv->allocated_views, view);
}

static void
bamf_factory_register_view (BamfFactory *self, BamfView *view, const char *path)
{
  g_return_if_fail (BAMF_IS_VIEW (view));
  g_return_if_fail (path != NULL);

  g_object_ref_sink (view);
  g_hash_table_insert (self->priv->open_views, g_strdup (path), view);
  g_signal_connect_after (G_OBJECT (view), BAMF_VIEW_SIGNAL_CLOSED,
                          G_CALLBACK (on_view_closed), self);
}

BamfApplication *
_bamf_factory_app_for_file (BamfFactory * factory,
                            const char * path,
                            gboolean create)
{
  BamfApplication *result = NULL, *app;
  GList *l;

  /* check if result is available in known allocated_views */
  for (l = factory->priv->allocated_views; l; l = l->next)
    {
      if (!BAMF_IS_APPLICATION (l->data))
        continue;

      app = BAMF_APPLICATION (l->data);
      if (g_strcmp0 (bamf_application_get_desktop_file (app), path) == 0)
        {
          result = app;
          break;
        }
    }

  /* else create new */
  if (!result && create)
    {
      /* delay registration until match time */
      result = bamf_application_new_favorite (path);

      if (BAMF_IS_APPLICATION (result))
        {
          bamf_factory_track_view (factory, BAMF_VIEW (result));
        }
    }

  return result;
}

static
BamfFactoryViewType compute_factory_type_by_str (const char *type)
{
  BamfFactoryViewType factory_type = BAMF_FACTORY_NONE;

  if (type && type[0] != '\0')
    {
      if (g_strcmp0 (type, "window") == 0)
        {
          factory_type = BAMF_FACTORY_WINDOW;
        }
      else if (g_strcmp0 (type, "application") == 0)
        {
          factory_type = BAMF_FACTORY_APPLICATION;
        }
      else if (g_strcmp0 (type, "tab") == 0)
        {
          factory_type = BAMF_FACTORY_TAB;
        }
      else if (g_strcmp0 (type, "view") == 0)
        {
          factory_type = BAMF_FACTORY_VIEW;
        }
    }

  return factory_type;
}

BamfView *
_bamf_factory_view_for_path (BamfFactory * factory, const char * path)
{
  return _bamf_factory_view_for_path_type (factory, path, BAMF_FACTORY_NONE);
}

BamfView *
_bamf_factory_view_for_path_type_str (BamfFactory * factory, const char * path,
                                                             const char * type)
{
  g_return_val_if_fail (BAMF_IS_FACTORY (factory), NULL);
  BamfFactoryViewType factory_type = compute_factory_type_by_str (type);

  return _bamf_factory_view_for_path_type (factory, path, factory_type);
}

BamfView *
_bamf_factory_view_for_path_type (BamfFactory * factory, const char * path,
                                                         BamfFactoryViewType type)
{
  GHashTable *views;
  BamfView *view;
  BamfDBusItemView *vproxy;
  GList *l;

  g_return_val_if_fail (BAMF_IS_FACTORY (factory), NULL);

  if (!path || path[0] == '\0')
    return NULL;

  views = factory->priv->open_views;
  view = g_hash_table_lookup (views, path);

  if (BAMF_IS_VIEW (view))
    return view;

  if (type == BAMF_FACTORY_NONE)
    {
      vproxy = _bamf_dbus_item_view_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                            G_DBUS_PROXY_FLAGS_NONE,
                                                            BAMF_DBUS_SERVICE_NAME,
                                                            path, NULL, NULL);
      if (G_IS_DBUS_PROXY (vproxy))
        {
          char *type_str = NULL;
          g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (vproxy), BAMF_DBUS_DEFAULT_TIMEOUT);
          _bamf_dbus_item_view_call_view_type_sync (vproxy, &type_str, NULL, NULL);
          type = compute_factory_type_by_str (type_str);
          g_free (type_str);
          g_object_unref (vproxy);
        }
    }

  switch (type)
    {
      case BAMF_FACTORY_VIEW:
        view = g_object_new (BAMF_TYPE_VIEW, NULL);
        break;
      case BAMF_FACTORY_WINDOW:
        view = BAMF_VIEW (bamf_window_new (path));
        break;
      case BAMF_FACTORY_APPLICATION:
        view = BAMF_VIEW (bamf_application_new (path));
        break;
      case BAMF_FACTORY_TAB:
        view = BAMF_VIEW (bamf_tab_new (path));
        break;
      case BAMF_FACTORY_NONE:
        view = NULL;
        break;
    }

  BamfView *matched_view = NULL;

  /* handle case where another allocated (but closed) view exists and the new
   * one matches it, so that we can reuse it. */
  if (BAMF_IS_APPLICATION (view))
    {
      const char *local_desktop_file = bamf_application_get_desktop_file (BAMF_APPLICATION (view));
      GList *local_children = _bamf_application_get_cached_xids (BAMF_APPLICATION (view));
      char *local_name = bamf_view_get_name (view);
      gboolean matched_by_name = FALSE;

      for (l = factory->priv->allocated_views; l; l = l->next)
        {
          if (!BAMF_IS_APPLICATION (l->data))
            continue;

          if (!bamf_view_is_closed (l->data))
            continue;

          BamfView *list_view = BAMF_VIEW (l->data);
          BamfApplication *list_app = BAMF_APPLICATION (l->data);

          const char *list_desktop_file = bamf_application_get_desktop_file (list_app);

          /* We try to match applications by desktop files */
          if (local_desktop_file && g_strcmp0 (local_desktop_file, list_desktop_file) == 0)
            {
              matched_view = list_view;
              break;
            }

          /* If the primary search doesn't give out any result, we fallback
           * to children window comparison */
          if (!list_desktop_file)
            {
              GList *list_children, *ll;
              list_children = _bamf_application_get_cached_xids (list_app);

              for (ll = local_children; ll; ll = ll->next)
                {
                  if (g_list_find (list_children, ll->data))
                    {
                      /* Not stopping the parent loop here is intended, as we
                       * can still find a better result in next iterations */
                      matched_view = list_view;
                      break;
                    }
                }

              if ((!matched_view || matched_by_name) && local_name && local_name[0] != '\0')
                {
                  char *list_name = bamf_view_get_name (list_view);
                  if (g_strcmp0 (local_name, list_name) == 0)
                    {
                      if (!matched_by_name)
                        {
                          matched_view = list_view;
                          matched_by_name = TRUE;
                        }
                      else
                        {
                          /* We have already matched an app by its name, this
                           * means that there are two apps with the same name.
                           * It's safer to ignore both, then. */
                          matched_view = NULL;
                        }
                    }

                  g_free (list_name);
                }
            }
        }

      g_free (local_name);
    }
  else if (BAMF_IS_WINDOW (view))
    {
      guint32 local_xid = bamf_window_get_xid (BAMF_WINDOW (view));

      for (l = factory->priv->allocated_views; l; l = l->next)
        {
          if (!BAMF_IS_WINDOW (l->data))
            continue;

          if (!bamf_view_is_closed (l->data))
            continue;

          BamfView *list_view = BAMF_VIEW (l->data);
          BamfWindow *list_win = BAMF_WINDOW (l->data);

          guint32 list_xid = bamf_window_get_xid (list_win);

          /* We try to match windows by xid */
          if (local_xid != 0 && local_xid == list_xid)
            {
              matched_view = list_view;
              break;
            }
        }
    }

  if (matched_view)
    {
      if (view)
        {
          /* We don't need anymore the view we've just created, let's forget it */
          g_object_unref (view);
        }

      /* If we are here, we're pretty sure that the view is not in the
       * open_views, hash-table (since it has been closed) so we can safely
       * re-register it here again. */
      view = matched_view;
      _bamf_view_set_path (view, path);
      bamf_factory_register_view (factory, view, path);
    }
  else if (view)
    {
      /* It's the first time we register this view, we also have to track it, then */
      bamf_factory_track_view (factory, view);
      bamf_factory_register_view (factory, view, path);
    }

  return view;
}

BamfFactory *
_bamf_factory_get_default (void)
{
  if (BAMF_IS_FACTORY (static_factory))
    return static_factory;

  static_factory = (BamfFactory *) g_object_new (BAMF_TYPE_FACTORY, NULL);
  return static_factory;
}
