/**
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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

#include "idoprogressmenuitem.h"
#include "idobasicmenuitem.h"
#include "idoactionhelper.h"

static void
on_progress_action_state_changed (IdoActionHelper * helper,
                                  GVariant        * state,
                                  gpointer          unused G_GNUC_UNUSED)
{
  IdoBasicMenuItem * ido_menu_item;
  char * str;

  ido_menu_item = IDO_BASIC_MENU_ITEM (ido_action_helper_get_widget (helper));

  g_return_if_fail (ido_menu_item != NULL);
  g_return_if_fail (g_variant_is_of_type (state, G_VARIANT_TYPE_UINT32));

  str = g_strdup_printf ("%"G_GUINT32_FORMAT"%%", g_variant_get_uint32 (state));
  ido_basic_menu_item_set_secondary_text (ido_menu_item, str);
  g_free (str);
}

/**
 * ido_progress_menu_item_new_from_model:
 * @menu_item: the corresponding menuitem
 * @actions: action group to tell when this GtkMenuItem is activated
 *
 * Creates a new progress menuitem with properties initialized from
 * the menuitem's attributes.
 *
 * If the menuitem's 'action' attribute is set, trigger that action
 * in @actions when this IdoBasicMenuItem is activated.
 */
GtkMenuItem *
ido_progress_menu_item_new_from_model (GMenuItem    * menu_item,
                                       GActionGroup * actions)
{
  guint i;
  guint n;
  gchar * str;
  IdoBasicMenuItem * ido_menu_item;
  GParameter parameters[4];

  /* create the ido menuitem */;

  n = 0;

  if (g_menu_item_get_attribute (menu_item, "label", "s", &str))
    {
      GParameter p = { "text", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_STRING);
      g_value_take_string (&p.value, str);
      parameters[n++] = p;
    }

  g_assert (n <= G_N_ELEMENTS (parameters));
  ido_menu_item = g_object_newv (IDO_TYPE_BASIC_MENU_ITEM, n, parameters);

  for (i=0; i<n; i++)
    g_value_unset (&parameters[i].value);

  /* give it an ActionHelper */

  if (g_menu_item_get_attribute (menu_item, "action", "s", &str))
    {
      IdoActionHelper * helper;

      helper = ido_action_helper_new (GTK_WIDGET(ido_menu_item),
                                      actions,
                                      str,
                                      NULL);
      g_signal_connect (helper, "action-state-changed",
                        G_CALLBACK (on_progress_action_state_changed), NULL);
      g_signal_connect_swapped (ido_menu_item, "destroy",
                                G_CALLBACK (g_object_unref), helper);

      g_free (str);
    }

  return GTK_MENU_ITEM (ido_menu_item);
}

