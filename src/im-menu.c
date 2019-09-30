/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#include "im-menu.h"

struct _ImMenuPrivate
{
  GMenu *toplevel_menu;
  GMenu *menu;
  ImApplicationList *applist;
};

G_DEFINE_TYPE_WITH_PRIVATE (ImMenu, im_menu, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_APPLICATION_LIST,
  NUM_PROPERTIES
};

static void
im_menu_finalize (GObject *object)
{
  ImMenuPrivate *priv = im_menu_get_instance_private (IM_MENU (object));

  g_object_unref (priv->toplevel_menu);
  g_object_unref (priv->menu);
  g_object_unref (priv->applist);

  G_OBJECT_CLASS (im_menu_parent_class)->finalize (object);
}

static void
im_menu_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  ImMenuPrivate *priv = im_menu_get_instance_private (IM_MENU (object));

  switch (property_id)
    {
    case PROP_APPLICATION_LIST:
      g_value_set_object (value, priv->applist);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
im_menu_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  ImMenuPrivate *priv = im_menu_get_instance_private (IM_MENU (object));

  switch (property_id)
    {
    case PROP_APPLICATION_LIST: /* construct only */
      priv->applist = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
im_menu_class_init (ImMenuClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = im_menu_finalize;
  object_class->get_property = im_menu_get_property;
  object_class->set_property = im_menu_set_property;

  g_object_class_install_property (object_class, PROP_APPLICATION_LIST,
                                   g_param_spec_object ("application-list", "", "",
                                                        IM_TYPE_APPLICATION_LIST,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
im_menu_init (ImMenu *menu)
{
  ImMenuPrivate *priv = im_menu_get_instance_private (menu);
  GMenuItem *root;

  priv->toplevel_menu = g_menu_new ();
  priv->menu = g_menu_new ();

  root = g_menu_item_new (NULL, "indicator.messages");
  g_menu_item_set_attribute (root, "x-canonical-type", "s", "com.canonical.indicator.root");
  g_menu_item_set_attribute (root, "action-namespace", "s", "indicator");
  g_menu_item_set_submenu (root, G_MENU_MODEL (priv->menu));
  g_menu_append_item (priv->toplevel_menu, root);

  g_object_unref (root);
}

ImApplicationList *
im_menu_get_application_list (ImMenu *menu)
{
  ImMenuPrivate *priv;

  g_return_val_if_fail (IM_IS_MENU (menu), FALSE);

  priv = im_menu_get_instance_private (menu);
  return priv->applist;
}

gboolean
im_menu_export (ImMenu           *menu,
                GDBusConnection  *connection,
                const gchar      *object_path,
                GError          **error)
{
  ImMenuPrivate *priv;

  g_return_val_if_fail (IM_IS_MENU (menu), FALSE);

  priv = im_menu_get_instance_private (menu);
  return g_dbus_connection_export_menu_model (connection,
                                              object_path,
                                              G_MENU_MODEL (priv->toplevel_menu),
                                              error) > 0;
}

void
im_menu_prepend_section (ImMenu     *menu,
                         GMenuModel *section)
{
  ImMenuPrivate *priv;

  g_return_if_fail (IM_IS_MENU (menu));
  g_return_if_fail (G_IS_MENU_MODEL (section));

  priv = im_menu_get_instance_private (menu);

  g_menu_prepend_section (priv->menu, NULL, section);
}

void
im_menu_append_section (ImMenu     *menu,
                        GMenuModel *section)
{
  ImMenuPrivate *priv;

  g_return_if_fail (IM_IS_MENU (menu));
  g_return_if_fail (G_IS_MENU_MODEL (section));

  priv = im_menu_get_instance_private (menu);

  g_menu_append_section (priv->menu, NULL, section);
}

/*
 * Inserts @item into @menu by comparing its
 * "x-messaging-menu-sort-string" with those found in existing menu
 * items between positions @first and @last.
 *
 * If @last is negative, it is counted from the end of @menu.
 */
void
im_menu_insert_item_sorted (ImMenu    *menu,
                            GMenuItem *item,
                            gint       first,
                            gint       last)
{
  ImMenuPrivate *priv;
  gint position = first;
  gchar *sort_string;

  g_return_if_fail (IM_IS_MENU (menu));
  g_return_if_fail (G_IS_MENU_ITEM (item));

  priv = im_menu_get_instance_private (menu);

  if (last < 0)
    last = g_menu_model_get_n_items (G_MENU_MODEL (priv->menu)) + last;

  g_return_if_fail (first <= last);

  if (g_menu_item_get_attribute (item, "x-messaging-menu-sort-string", "s", &sort_string))
    {
      while (position < last)
        {
          gchar *item_sort;

          if (g_menu_model_get_item_attribute(G_MENU_MODEL(priv->menu), position, "x-messaging-menu-sort-string", "s", &item_sort))
            {
              gint cmp;

              cmp = g_utf8_collate(sort_string, item_sort);
              g_free (item_sort);
              if (cmp < 0)
                break;
            }

          position++;
        }
    }

  g_menu_insert_item (priv->menu, position, item);
}
