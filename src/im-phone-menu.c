/*
 * Copyright 2012 Canonical Ltd.
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

#include "im-phone-menu.h"

#include <string.h>
#include <glib/gi18n.h>

typedef ImMenuClass ImPhoneMenuClass;

struct _ImPhoneMenu
{
  ImMenu parent;

  GMenu *message_section;
  GMenu *source_section;
  GMenu *clear_section;
};

G_DEFINE_TYPE (ImPhoneMenu, im_phone_menu, IM_TYPE_MENU);

typedef void (*ImMenuForeachFunc) (GMenuModel *menu, gint pos);

static void
im_phone_menu_foreach_item_with_action (GMenuModel        *menu,
                                        const gchar       *action,
                                        ImMenuForeachFunc  func)
{
  gint n_items;
  gint i;

  n_items = g_menu_model_get_n_items (menu);
  for (i = 0; i < n_items; i++)
    {
      gchar *item_action;

      g_menu_model_get_item_attribute (menu, i, G_MENU_ATTRIBUTE_ACTION, "s", &item_action);

      if (g_str_equal (action, item_action))
        func (menu, i);

      g_free (item_action);
    }
}

static void
im_phone_menu_update_clear_section (ImPhoneMenu *menu)
{
  gboolean is_shown;
  gboolean should_be_shown;

  is_shown = g_menu_model_get_n_items (G_MENU_MODEL (menu->clear_section)) > 0;
  should_be_shown = (g_menu_model_get_n_items (G_MENU_MODEL (menu->message_section)) +
                     g_menu_model_get_n_items (G_MENU_MODEL (menu->source_section))) > 0;

  if (!is_shown && should_be_shown)
    {
      GMenuItem *item;

      item = g_menu_item_new (_("Clear All"), "remove-all");
      g_menu_item_set_attribute (item, "x-canonical-type", "s", "com.canonical.indicator.button");
      g_menu_append_item (menu->clear_section, item);

      g_object_unref (item);
    }
  else if (is_shown && !should_be_shown)
    {
      g_menu_remove (menu->clear_section, 0);
    }
}

static void
im_phone_menu_constructed (GObject *object)
{
  ImPhoneMenu *menu = IM_PHONE_MENU (object);
  ImApplicationList *applist;

  im_menu_append_section (IM_MENU (menu), G_MENU_MODEL (menu->message_section));
  im_menu_append_section (IM_MENU (menu), G_MENU_MODEL (menu->source_section));
  im_menu_append_section (IM_MENU (menu), G_MENU_MODEL (menu->clear_section));

  applist = im_menu_get_application_list (IM_MENU (menu));

  g_signal_connect_swapped (applist, "message-added", G_CALLBACK (im_phone_menu_add_message), menu);
  g_signal_connect_swapped (applist, "message-removed", G_CALLBACK (im_phone_menu_remove_message), menu);
  g_signal_connect_swapped (applist, "app-stopped", G_CALLBACK (im_phone_menu_remove_application), menu);
  g_signal_connect_swapped (applist, "remove-all", G_CALLBACK (im_phone_menu_remove_all), menu);

  G_OBJECT_CLASS (im_phone_menu_parent_class)->constructed (object);
}

static void
im_phone_menu_dispose (GObject *object)
{
  ImPhoneMenu *menu = IM_PHONE_MENU (object);

  g_clear_object (&menu->message_section);
  g_clear_object (&menu->source_section);
  g_clear_object (&menu->clear_section);

  G_OBJECT_CLASS (im_phone_menu_parent_class)->dispose (object);
}

static void
im_phone_menu_finalize (GObject *object)
{
  G_OBJECT_CLASS (im_phone_menu_parent_class)->finalize (object);
}

static void
im_phone_menu_class_init (ImPhoneMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = im_phone_menu_constructed;
  object_class->dispose = im_phone_menu_dispose;
  object_class->finalize = im_phone_menu_finalize;
}

static void
im_phone_menu_init (ImPhoneMenu *menu)
{
  menu->message_section = g_menu_new ();
  menu->source_section = g_menu_new ();
  menu->clear_section = g_menu_new ();
}

ImPhoneMenu *
im_phone_menu_new (ImApplicationList  *applist)
{
  g_return_val_if_fail (IM_IS_APPLICATION_LIST (applist), NULL);

  return g_object_new (IM_TYPE_PHONE_MENU,
                       "application-list", applist,
                       NULL);
}

static gint64
im_phone_menu_get_message_time (GMenuModel *model,
                                gint        i)
{
  gint64 time;

  g_menu_model_get_item_attribute (model, i, "x-canonical-time", "x", &time);

  return time;
}

void
im_phone_menu_add_message (ImPhoneMenu     *menu,
                           const gchar     *app_id,
                           GIcon           *app_icon,
                           const gchar     *id,
                           GVariant        *serialized_icon,
                           const gchar     *title,
                           const gchar     *subtitle,
                           const gchar     *body,
                           GVariant        *actions,
                           gint64           time)
{
  GMenuItem *item;
  gchar *action_name;
  gint n_messages;
  gint pos;
  GVariant *serialized_app_icon;

  g_return_if_fail (IM_IS_PHONE_MENU (menu));
  g_return_if_fail (app_id);

  action_name = g_strconcat (app_id, ".msg.", id, NULL);

  item = g_menu_item_new (title, NULL);
  g_menu_item_set_action_and_target_value (item, action_name, g_variant_new_boolean (TRUE));

  g_menu_item_set_attribute (item, "x-canonical-type", "s", "com.canonical.indicator.messages.messageitem");
  g_menu_item_set_attribute (item, "x-canonical-message-id", "s", id);
  g_menu_item_set_attribute (item, "x-canonical-subtitle", "s", subtitle);
  g_menu_item_set_attribute (item, "x-canonical-text", "s", body);
  g_menu_item_set_attribute (item, "x-canonical-time", "x", time);

  if (serialized_icon)
    g_menu_item_set_attribute_value (item, "icon", serialized_icon);

  if (app_icon && (serialized_app_icon = g_icon_serialize (app_icon)))
    {
      g_menu_item_set_attribute_value (item, "x-canonical-app-icon", serialized_app_icon);
      g_variant_unref (serialized_app_icon);
    }

  if (actions)
    g_menu_item_set_attribute (item, "x-canonical-message-actions", "v", actions);

  n_messages = g_menu_model_get_n_items (G_MENU_MODEL (menu->message_section));
  pos = 0;
  while (pos < n_messages &&
         time < im_phone_menu_get_message_time (G_MENU_MODEL (menu->message_section), pos))
    pos++;

  g_menu_insert_item (menu->message_section, pos, item);

  im_phone_menu_update_clear_section (menu);

  g_free (action_name);
  g_object_unref (item);
}

void
im_phone_menu_remove_message (ImPhoneMenu     *menu,
                              const gchar     *app_id,
                              const gchar     *id)
{
  gchar *action_name;

  g_return_if_fail (IM_IS_PHONE_MENU (menu));
  g_return_if_fail (app_id != NULL);

  action_name = g_strconcat (app_id, ".msg.", id, NULL);
  im_phone_menu_foreach_item_with_action (G_MENU_MODEL (menu->message_section),
                                          action_name,
                                          (ImMenuForeachFunc) g_menu_remove);

  im_phone_menu_update_clear_section (menu);

  g_free (action_name);
}

void
im_phone_menu_add_source (ImPhoneMenu     *menu,
                          const gchar     *app_id,
                          const gchar     *id,
                          const gchar     *label,
                          const gchar     *iconstr)
{
  GMenuItem *item;
  gchar *action_name;

  g_return_if_fail (IM_IS_PHONE_MENU (menu));
  g_return_if_fail (app_id != NULL);

  action_name = g_strconcat (app_id, ".src.", id, NULL);

  item = g_menu_item_new (label, NULL);
  g_menu_item_set_action_and_target_value (item, action_name, g_variant_new_boolean (TRUE));
  g_menu_item_set_attribute (item, "x-canonical-type", "s", "com.canonical.indicator.messages.sourceitem");

  if (iconstr)
    g_menu_item_set_attribute (item, "x-canonical-icon", "s", iconstr);

  g_menu_prepend_item (menu->source_section, item);

  g_free (action_name);
  g_object_unref (item);
}

void
im_phone_menu_remove_source (ImPhoneMenu     *menu,
                             const gchar     *app_id,
                             const gchar     *id)
{
  gchar *action_name;

  g_return_if_fail (IM_IS_PHONE_MENU (menu));
  g_return_if_fail (app_id != NULL);

  action_name = g_strconcat (app_id, ".src.", id, NULL);
  im_phone_menu_foreach_item_with_action (G_MENU_MODEL (menu->source_section),
                                          action_name,
                                          (ImMenuForeachFunc) g_menu_remove);

  g_free (action_name);
}

static void
im_phone_menu_remove_all_for_app (GMenu           *menu,
                                  const gchar     *app_id)
{
  gchar *prefix;
  gint n_items;
  gint i = 0;

  prefix = g_strconcat (app_id, ".", NULL);

  n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));
  while (i < n_items)
    {
      gchar *action;

      g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, G_MENU_ATTRIBUTE_ACTION, "s", &action);
      if (g_str_has_prefix (action, prefix))
        {
          g_menu_remove (menu, i);
          n_items--;
        }
      else
        {
          i++;
        }

      g_free (action);
    }

  g_free (prefix);
}

void
im_phone_menu_remove_application (ImPhoneMenu     *menu,
                                  const gchar     *app_id)
{
  g_return_if_fail (IM_IS_PHONE_MENU (menu));
  g_return_if_fail (app_id != NULL);

  im_phone_menu_remove_all_for_app (menu->source_section, app_id);
  im_phone_menu_remove_all_for_app (menu->message_section, app_id);

  im_phone_menu_update_clear_section (menu);
}

void
im_phone_menu_remove_all (ImPhoneMenu *menu)
{
  g_return_if_fail (IM_IS_PHONE_MENU (menu));

  while (g_menu_model_get_n_items (G_MENU_MODEL (menu->message_section)))
    g_menu_remove (menu->message_section, 0);

  while (g_menu_model_get_n_items (G_MENU_MODEL (menu->source_section)))
    g_menu_remove (menu->source_section, 0);

  im_phone_menu_update_clear_section (menu);
}
