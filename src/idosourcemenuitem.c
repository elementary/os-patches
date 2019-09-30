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

#include "idosourcemenuitem.h"

#include <libintl.h>
#include "idodetaillabel.h"
#include "idoactionhelper.h"

typedef GtkMenuItemClass IdoSourceMenuItemClass;

struct _IdoSourceMenuItem
{
  GtkMenuItem parent;

  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *detail;

  gint64 time;
  guint timer_id;
};

G_DEFINE_TYPE (IdoSourceMenuItem, ido_source_menu_item, GTK_TYPE_MENU_ITEM);

static void
ido_source_menu_item_constructed (GObject *object)
{
  IdoSourceMenuItem *item = IDO_SOURCE_MENU_ITEM (object);
  GtkWidget *grid;
  gint icon_width;

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, NULL);

  item->icon = g_object_ref (gtk_image_new ());
  gtk_widget_set_margin_left (item->icon, icon_width);
  gtk_widget_set_margin_right (item->icon, 6);

  item->label = g_object_ref (gtk_label_new (""));
  gtk_label_set_max_width_chars (GTK_LABEL (item->label), 40);
  gtk_label_set_ellipsize (GTK_LABEL (item->label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (item->label), 0.0, 0.5);

  item->detail = g_object_ref (ido_detail_label_new (""));
  gtk_widget_set_halign (item->detail, GTK_ALIGN_END);
  gtk_widget_set_hexpand (item->detail, TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (item->detail), "accelerator");

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (grid), item->icon, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), item->label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), item->detail, 2, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (object), grid);
  gtk_widget_show_all (grid);

  G_OBJECT_CLASS (ido_source_menu_item_parent_class)->constructed (object);
}

static gchar *
ido_source_menu_item_time_span_string (gint64 timestamp)
{
  gchar *str;
  gint64 span;
  gint hours;
  gint minutes;

  span = MAX (g_get_real_time () - timestamp, 0) / G_USEC_PER_SEC;
  hours = span / 3600;
  minutes = (span / 60) % 60;

  if (hours == 0)
    {
      /* TRANSLATORS: number of minutes that have passed */
      str = g_strdup_printf (ngettext ("%d min", "%d min", minutes), minutes);
    }
  else
    {
      /* TRANSLATORS: number of hours that have passed */
      str = g_strdup_printf (ngettext ("%d h", "%d h", hours), hours);
    }

  return str;
}

static void
ido_source_menu_item_set_detail_time (IdoSourceMenuItem *self,
                                      gint64             time)
{
  gchar *str;

  self->time = time;

  str = ido_source_menu_item_time_span_string (self->time);
  ido_detail_label_set_text (IDO_DETAIL_LABEL (self->detail), str);

  g_free (str);
}

static gboolean
ido_source_menu_item_update_time (gpointer data)
{
  IdoSourceMenuItem *self = data;

  ido_source_menu_item_set_detail_time (self, self->time);

  return TRUE;
}

static void
ido_source_menu_item_dispose (GObject *object)
{
  IdoSourceMenuItem *self = IDO_SOURCE_MENU_ITEM (object);

  if (self->timer_id != 0)
    {
      g_source_remove (self->timer_id);
      self->timer_id = 0;
    }

  g_clear_object (&self->icon);
  g_clear_object (&self->label);
  g_clear_object (&self->detail);

  G_OBJECT_CLASS (ido_source_menu_item_parent_class)->dispose (object);
}

static void
ido_source_menu_item_class_init (IdoSourceMenuItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ido_source_menu_item_constructed;
  object_class->dispose = ido_source_menu_item_dispose;
}

static void
ido_source_menu_item_init (IdoSourceMenuItem *self)
{
}

static void
ido_source_menu_item_set_label (IdoSourceMenuItem *item,
                                const gchar       *label)
{
  gtk_label_set_label (GTK_LABEL (item->label), label ? label : "");
}

static void
ido_source_menu_item_set_icon (IdoSourceMenuItem *item,
                               GIcon             *icon)
{
  if (icon)
    gtk_image_set_from_gicon (GTK_IMAGE (item->icon), icon, GTK_ICON_SIZE_MENU);
  else
    gtk_image_clear (GTK_IMAGE (item->icon));
}


static void
ido_source_menu_item_activate (GtkMenuItem *item,
                               gpointer     user_data)
{
  IdoActionHelper *helper = user_data;

  /* The parameter signifies whether this source was activated (TRUE) or
   * dismissed (FALSE). Since there's no UI to dismiss a gtkmenuitem,
   * this always passes TRUE. */
  ido_action_helper_activate_with_parameter (helper, g_variant_new_boolean (TRUE));
}

static void
ido_source_menu_item_state_changed (IdoActionHelper *helper,
                                    GVariant        *state,
                                    gpointer         user_data)
{
  IdoSourceMenuItem *item = user_data;
  guint32 count;
  gint64 time;
  const gchar *str;

  if (item->timer_id != 0)
    {
      g_source_remove (item->timer_id);
      item->timer_id = 0;
    }

  g_return_val_if_fail (g_variant_is_of_type (state, G_VARIANT_TYPE ("(uxsb)")), FALSE);

  g_variant_get (state, "(ux&sb)", &count, &time, &str, NULL);

  if (count != 0)
    ido_detail_label_set_count (IDO_DETAIL_LABEL (item->detail), count);
  else if (time != 0)
    {
      ido_source_menu_item_set_detail_time (item, time);
      item->timer_id = g_timeout_add_seconds (59, ido_source_menu_item_update_time, item);
    }
  else if (str != NULL && *str)
    ido_detail_label_set_text (IDO_DETAIL_LABEL (item->detail), str);
}

GtkMenuItem *
ido_source_menu_item_new_from_menu_model (GMenuItem    *menuitem,
                                          GActionGroup *actions)
{
  GtkMenuItem *item;
  GVariant *serialized_icon;
  GIcon *icon = NULL;
  gchar *label;
  gchar *action = NULL;

  item = g_object_new (IDO_TYPE_SOURCE_MENU_ITEM, NULL);

  if (g_menu_item_get_attribute (menuitem, "label", "s", &label))
    {
      ido_source_menu_item_set_label (IDO_SOURCE_MENU_ITEM (item), label);
      g_free (label);
    }

  serialized_icon = g_menu_item_get_attribute_value (menuitem, "icon", NULL);
  if (serialized_icon)
    {
      icon = g_icon_deserialize (serialized_icon);
      g_variant_unref (serialized_icon);
    }
  ido_source_menu_item_set_icon (IDO_SOURCE_MENU_ITEM (item), icon);

  if (g_menu_item_get_attribute (menuitem, "action", "s", &action))
    {
      IdoActionHelper *helper;

      helper = ido_action_helper_new (GTK_WIDGET (item), actions, action, NULL);
      g_signal_connect (helper, "action-state-changed",
                        G_CALLBACK (ido_source_menu_item_state_changed), item);
      g_signal_connect_object (item, "activate",
                               G_CALLBACK (ido_source_menu_item_activate), helper,
                               0);
      g_signal_connect_swapped (item, "destroy", G_CALLBACK (g_object_unref), helper);

      g_free (action);
    }

  if (icon)
    g_object_unref (icon);

  return item;
}
