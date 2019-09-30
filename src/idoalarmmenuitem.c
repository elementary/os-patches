/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *   Ted Gould <ted@canonical.com>
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
 */

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <gtk/gtk.h>

#include "idoactionhelper.h"
#include "idotimestampmenuitem.h"

/**
 * ido_alarm_menu_item_new_from_model:
 * @menu_item: the corresponding menuitem
 * @actions: action group to tell when this GtkMenuItem is activated
 *
 * Creates a new IdoTimeStampMenuItem with properties initialized
 * appropriately for a com.canonical.indicator.alarm
 *
 * If the menuitem's 'action' attribute is set, trigger that action
 * in @actions when this IdoAppointmentMenuItem is activated.
 */
GtkMenuItem *
ido_alarm_menu_item_new_from_model (GMenuItem    * menu_item,
                                    GActionGroup * actions)
{
  guint i;
  guint n;
  gint64 i64;
  gchar * str;
  IdoBasicMenuItem * ido_menu_item;
  GParameter parameters[8];

  /* create the ido_menu_item */

  n = 0;

  if (g_menu_item_get_attribute (menu_item, G_MENU_ATTRIBUTE_LABEL, "s", &str))
    {
      GParameter p = { "text", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_STRING);
      g_value_take_string (&p.value, str);
      parameters[n++] = p;
    }

  if (TRUE)
    {
      GParameter p = { "icon", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_OBJECT);
      g_value_take_object (&p.value, g_themed_icon_new_with_default_fallbacks ("alarm-symbolic"));
      parameters[n++] = p;
    }

  if (g_menu_item_get_attribute (menu_item, "x-canonical-time-format", "s", &str))
    {
      GParameter p = { "format", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_STRING);
      g_value_take_string (&p.value, str);
      parameters[n++] = p;
    }

  if (g_menu_item_get_attribute (menu_item, "x-canonical-time", "x", &i64))
    {
      GParameter p = { "date-time", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_DATE_TIME);
      g_value_take_boxed (&p.value, g_date_time_new_from_unix_local (i64));
      parameters[n++] = p;
    }

  g_assert (n <= G_N_ELEMENTS (parameters));
  ido_menu_item = g_object_newv (IDO_TYPE_TIME_STAMP_MENU_ITEM, n, parameters);

  for (i=0; i<n; i++)
    g_value_unset (&parameters[i].value);


  /* add an ActionHelper */

  if (g_menu_item_get_attribute (menu_item, "action", "s", &str))
    {
      GVariant * target;
      IdoActionHelper * helper;

      target = g_menu_item_get_attribute_value (menu_item, "target",
                                                G_VARIANT_TYPE_ANY);
      helper = ido_action_helper_new (GTK_WIDGET(ido_menu_item), actions,
                                      str, target);
      g_signal_connect_swapped (ido_menu_item, "activate",
                                G_CALLBACK (ido_action_helper_activate), helper);
      g_signal_connect_swapped (ido_menu_item, "destroy",
                                G_CALLBACK (g_object_unref), helper);

      g_clear_pointer (&target, g_variant_unref);
      g_free (str);
    }

  return GTK_MENU_ITEM (ido_menu_item);
}
