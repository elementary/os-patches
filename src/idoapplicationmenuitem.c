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

#include "idoapplicationmenuitem.h"
#include "idoactionhelper.h"

typedef GtkMenuItemClass IdoApplicationMenuItemClass;

struct _IdoApplicationMenuItem
{
  GtkMenuItem parent;

  gboolean is_running;

  GtkWidget *icon;
  GtkWidget *label;
};

G_DEFINE_TYPE (IdoApplicationMenuItem, ido_application_menu_item, GTK_TYPE_MENU_ITEM);

static void
ido_application_menu_item_constructed (GObject *object)
{
  IdoApplicationMenuItem *item = IDO_APPLICATION_MENU_ITEM (object);
  GtkWidget *grid;
  gint icon_height;

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &icon_height);

  item->icon = g_object_ref (gtk_image_new ());
  gtk_image_set_pixel_size (GTK_IMAGE (item->icon), icon_height);
  gtk_widget_set_margin_right (item->icon, 6);

  item->label = g_object_ref (gtk_label_new (""));

  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (grid), item->icon, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), item->label, 1, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (object), grid);
  gtk_widget_show_all (grid);

  G_OBJECT_CLASS (ido_application_menu_item_parent_class)->constructed (object);
}

static void
ido_application_menu_item_dispose (GObject *object)
{
  IdoApplicationMenuItem *self = IDO_APPLICATION_MENU_ITEM (object);

  g_clear_object (&self->icon);
  g_clear_object (&self->label);

  G_OBJECT_CLASS (ido_application_menu_item_parent_class)->dispose (object);
}

static gboolean
ido_application_menu_item_draw (GtkWidget *widget,
                                cairo_t   *cr)
{
  IdoApplicationMenuItem *item = IDO_APPLICATION_MENU_ITEM (widget);

  GTK_WIDGET_CLASS (ido_application_menu_item_parent_class)->draw (widget, cr);

  if (item->is_running)
    {
      const int arrow_width = 5;
      const double half_arrow_height = 4.5;
      GtkAllocation alloc;
      GdkRGBA color;
      double center;

      gtk_widget_get_allocation (widget, &alloc);

      gtk_style_context_get_color (gtk_widget_get_style_context (widget),
                                   gtk_widget_get_state_flags (widget),
                                   &color);
      gdk_cairo_set_source_rgba (cr, &color);

      center = alloc.height / 2 + 0.5;

      cairo_move_to (cr, 0, center - half_arrow_height);
      cairo_line_to (cr, 0, center + half_arrow_height);
      cairo_line_to (cr, arrow_width, center);
      cairo_close_path (cr);

      cairo_fill (cr);
    }

  return FALSE;
}

void
ido_application_menu_item_class_init (IdoApplicationMenuItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ido_application_menu_item_constructed;
  object_class->dispose = ido_application_menu_item_dispose;

  widget_class->draw = ido_application_menu_item_draw;
}

static void
ido_application_menu_item_init (IdoApplicationMenuItem *self)
{
}

static void
ido_application_menu_item_set_label (IdoApplicationMenuItem *item,
                                     const gchar            *label)
{
  gtk_label_set_label (GTK_LABEL (item->label), label);
}

static void
ido_application_menu_item_set_icon (IdoApplicationMenuItem *item,
                                    GIcon                  *icon)
{
  gtk_image_set_from_gicon (GTK_IMAGE (item->icon), icon, GTK_ICON_SIZE_MENU);
}

static void
ido_application_menu_item_state_changed (IdoActionHelper *helper,
                                         GVariant        *state,
                                         gpointer         user_data)
{
  IdoApplicationMenuItem *item = user_data;

  item->is_running = g_variant_get_boolean (state);
  gtk_widget_queue_draw (GTK_WIDGET (item));
}

GtkMenuItem *
ido_application_menu_item_new_from_model (GMenuItem    *menuitem,
                                          GActionGroup *actions)
{
  GtkMenuItem *item;
  gchar *label;
  GVariant *serialized_icon;
  gchar *action;

  item = g_object_new (IDO_TYPE_APPLICATION_MENU_ITEM, NULL);

  if (g_menu_item_get_attribute (menuitem, "label", "s", &label))
    {
      ido_application_menu_item_set_label (IDO_APPLICATION_MENU_ITEM (item), label);
      g_free (label);
    }

  serialized_icon = g_menu_item_get_attribute_value (menuitem, "icon", NULL);
  if (serialized_icon)
    {
      GIcon *icon;

      icon = g_icon_deserialize (serialized_icon);
      if (icon)
        {
          ido_application_menu_item_set_icon (IDO_APPLICATION_MENU_ITEM (item), icon);
          g_object_unref (icon);
        }

      g_variant_unref (serialized_icon);
    }

  if (g_menu_item_get_attribute (menuitem, "action", "s", &action))
    {
      IdoActionHelper *helper;

      helper = ido_action_helper_new (GTK_WIDGET (item), actions, action, NULL);
      g_signal_connect (helper, "action-state-changed",
                        G_CALLBACK (ido_application_menu_item_state_changed), item);
      g_signal_connect_object (item, "activate",
                               G_CALLBACK (ido_action_helper_activate), helper,
                               G_CONNECT_SWAPPED);
      g_signal_connect_swapped (item, "destroy", G_CALLBACK (g_object_unref), helper);

      g_free (action);
    }

  return item;
}
