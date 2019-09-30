/*
 * A GtkCheckMenuItem that uses a GtkSwitch to show its 'active' property
 *
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Charles Kerr <charles.kerr@canonical.com>
 */

#include "config.h"

#include "idoswitchmenuitem.h"
#include "idoactionhelper.h"

static gboolean ido_switch_menu_button_release_event (GtkWidget      * widget,
                                                      GdkEventButton * event);


struct _IdoSwitchMenuItemPrivate
{
  GtkWidget * box;
  GtkWidget * content_area;
  GtkWidget * label;
  GtkWidget * image;
  GtkWidget * switch_w;
};

/***
****  Life Cycle
***/

G_DEFINE_TYPE (IdoSwitchMenuItem, ido_switch_menu_item, GTK_TYPE_CHECK_MENU_ITEM)

static void
ido_switch_menu_item_class_init (IdoSwitchMenuItemClass *klass)
{
  GObjectClass * gobject_class;
  GtkWidgetClass * widget_class;
  GtkCheckMenuItemClass * check_class;

  gobject_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (gobject_class, sizeof (IdoSwitchMenuItemPrivate));

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->button_release_event = ido_switch_menu_button_release_event;

  check_class = GTK_CHECK_MENU_ITEM_CLASS (klass);
  check_class->draw_indicator = NULL;
}

static void
ido_switch_menu_item_init (IdoSwitchMenuItem *item)
{
  IdoSwitchMenuItemPrivate *priv;

  priv = item->priv = G_TYPE_INSTANCE_GET_PRIVATE (item, IDO_TYPE_SWITCH_MENU_ITEM, IdoSwitchMenuItemPrivate);
  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->content_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  priv->switch_w = gtk_switch_new ();

  gtk_box_pack_start (GTK_BOX (priv->box), priv->content_area, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (priv->box), priv->switch_w, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (item), priv->box);
  gtk_widget_show_all (priv->box);

  g_object_bind_property (item, "active",
                          priv->switch_w, "active",
                          G_BINDING_SYNC_CREATE);
}

/***
**** Don't popdown the menu immediately after clicking on a switch...
**** wait a moment so the user can see the GtkSwitch be toggled.
***/

static gboolean
popdown_later_cb (gpointer widget)
{
  GtkWidget * parent = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU (parent))
    {
      gtk_menu_shell_deactivate (GTK_MENU_SHELL(parent));
    }
  g_object_unref (widget);
  return FALSE; /* only call this cb once */
}

static gboolean
ido_switch_menu_button_release_event (GtkWidget * widget, GdkEventButton * event)
{
  gtk_menu_item_activate (GTK_MENU_ITEM(widget));
  g_timeout_add (500, popdown_later_cb, g_object_ref(widget));
  return TRUE; /* stop the event so that it doesn't trigger popdown() */
}

/**
 * ido_switch_menu_item_new:
 *
 * Creates a new #IdoSwitchMenuItem
 *
 * Return Value: a new #IdoSwitchMenuItem.
 **/
GtkWidget *
ido_switch_menu_item_new (void)
{
  return g_object_new (IDO_TYPE_SWITCH_MENU_ITEM, NULL);
}

/**
 * ido_switch_menu_item_get_content_area:
 * @item: The #IdoSwitchMenuItem.
 *
 * Get the #GtkContainer to add additional widgets into.
 *
 * This function is dperecated.
 *
 * Return Value: (transfer none): The #GtkContainer to add additional widgets into.
 **/
GtkContainer *
ido_switch_menu_item_get_content_area (IdoSwitchMenuItem * item)
{
  static gboolean warned = FALSE;

  g_return_val_if_fail (IDO_IS_SWITCH_MENU_ITEM(item), NULL);

  if (!warned)
    {
      g_warning ("%s is deprecated. Please don't use it, especially if you're using"
                 "ido_switch_menu_set_{label,icon}()", G_STRFUNC);
      warned = TRUE;
    }

  return GTK_CONTAINER (item->priv->content_area);
}

/**
 * ido_switch_menu_item_set_label:
 * @item: a #IdoSwitchMenuItem.
 * @label: a string to set as the label of @item
 *
 * Set the label of @item to @label.
 **/
void
ido_switch_menu_item_set_label (IdoSwitchMenuItem *item,
                                const gchar       *label)
{
  IdoSwitchMenuItemPrivate *priv;

  g_return_if_fail (IDO_IS_SWITCH_MENU_ITEM (item));
  g_return_if_fail (label != NULL);

  priv = item->priv;

  if (priv->label == NULL)
    {
      priv->label = gtk_label_new (NULL);
      gtk_widget_set_halign (priv->label, GTK_ALIGN_START);
      gtk_widget_show (priv->label);
      gtk_box_pack_end (GTK_BOX (priv->content_area), priv->label, TRUE, TRUE, 0);
    }

  gtk_label_set_text (GTK_LABEL (priv->label), label);
}

/**
 * ido_switch_menu_item_set_icon:
 * @item: a #IdoSwitchMenuItem.
 * @icon: (allow-none): a #GIcon 
 *
 * Set the icon of @item to @icon.
 **/
void
ido_switch_menu_item_set_icon (IdoSwitchMenuItem *item,
                               GIcon             *icon)
{
  IdoSwitchMenuItemPrivate *priv;

  g_return_if_fail (IDO_IS_SWITCH_MENU_ITEM (item));
  g_return_if_fail (icon == NULL || G_IS_ICON (icon));

  priv = item->priv;

  if (icon)
    {
      if (priv->image == NULL)
        {
          priv->image = gtk_image_new ();
          gtk_widget_show (priv->image);
          gtk_box_pack_start (GTK_BOX (priv->content_area), priv->image, FALSE, FALSE, 0);
        }

      gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, GTK_ICON_SIZE_MENU);
    }
  else if (priv->image)
    {
      gtk_image_clear (GTK_IMAGE (priv->image));
    }
}

static void
ido_source_menu_item_state_changed (IdoActionHelper *helper,
                                    GVariant        *state,
                                    gpointer         user_data)
{
  IdoSwitchMenuItem *item = user_data;

  if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    gtk_switch_set_active (GTK_SWITCH (item->priv->switch_w),
                           g_variant_get_boolean (state));
}

GtkMenuItem *
ido_switch_menu_item_new_from_menu_model (GMenuItem    *menuitem,
                                          GActionGroup *actions)
{
  GtkMenuItem *item;
  gchar *label;
  GVariant *serialized_icon;
  gchar *action = NULL;

  item = g_object_new (IDO_TYPE_SWITCH_MENU_ITEM, NULL);

  if (g_menu_item_get_attribute (menuitem, "label", "s", &label))
    {
      ido_switch_menu_item_set_label (IDO_SWITCH_MENU_ITEM (item), label);
      g_free (label);
    }

  serialized_icon = g_menu_item_get_attribute_value (menuitem, "icon", NULL);
  if (serialized_icon)
    {
      GIcon *icon;

      icon = g_icon_deserialize (serialized_icon);
      if (icon)
        {
          ido_switch_menu_item_set_icon (IDO_SWITCH_MENU_ITEM (item), icon);
          g_object_unref (icon);
        }

      g_variant_unref (serialized_icon);
    }

  if (g_menu_item_get_attribute (menuitem, "action", "s", &action))
    {
      IdoActionHelper *helper;

      helper = ido_action_helper_new (GTK_WIDGET (item), actions, action, NULL);
      g_signal_connect (helper, "action-state-changed",
                        G_CALLBACK (ido_source_menu_item_state_changed), item);
      g_signal_connect_object (item, "activate",
                               G_CALLBACK (ido_action_helper_activate), helper,
                               G_CONNECT_SWAPPED);
      g_signal_connect_swapped (item, "destroy", G_CALLBACK (g_object_unref), helper);

      g_free (action);
    }

  return item;
}
