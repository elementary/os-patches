/*
 * Copyright 2010 Canonical, Ltd.
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
 * Authors:
 *    Cody Russell <crussell@canonical.com>
 */

#include <gdk/gdkkeysyms.h>
#include "idoentrymenuitem.h"
#include "config.h"

static void     ido_entry_menu_item_select            (GtkMenuItem        *item);
static void     ido_entry_menu_item_deselect          (GtkMenuItem        *item);
static gboolean ido_entry_menu_item_button_release    (GtkWidget      *widget,
                                                       GdkEventButton *event);
static gboolean ido_entry_menu_item_key_press         (GtkWidget      *widget,
                                                       GdkEventKey    *event,
                                                       gpointer        data);
static gboolean ido_entry_menu_item_button_press      (GtkWidget      *widget,
                                                       GdkEventButton *event);
static void     ido_entry_menu_item_send_focus_change (GtkWidget      *widget,
                                                       gboolean        in);
static void     entry_realized_cb                     (GtkWidget        *widget,
                                                       IdoEntryMenuItem *item);
static void     entry_move_focus_cb                   (GtkWidget        *widget,
                                                       GtkDirectionType  direction,
                                                       IdoEntryMenuItem *item);

struct _IdoEntryMenuItemPrivate
{
  GtkWidget       *box;
  GtkWidget       *entry;
  gboolean         selected;
};

G_DEFINE_TYPE (IdoEntryMenuItem, ido_entry_menu_item, GTK_TYPE_MENU_ITEM)

#define IDO_ENTRY_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), IDO_TYPE_ENTRY_MENU_ITEM, IdoEntryMenuItemPrivate))

static void
ido_entry_menu_item_class_init (IdoEntryMenuItemClass *klass)
{
  GObjectClass     *gobject_class;
  GtkWidgetClass   *widget_class;
  GtkMenuItemClass *menu_item_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  menu_item_class = GTK_MENU_ITEM_CLASS (klass);

  widget_class->button_release_event = ido_entry_menu_item_button_release;
  widget_class->button_press_event = ido_entry_menu_item_button_press;

  menu_item_class->select = ido_entry_menu_item_select;
  menu_item_class->deselect = ido_entry_menu_item_deselect;

  menu_item_class->hide_on_activate = TRUE;

  g_type_class_add_private (gobject_class, sizeof (IdoEntryMenuItemPrivate));
}

static void
ido_entry_menu_item_init (IdoEntryMenuItem *item)
{
  IdoEntryMenuItemPrivate *priv;
  GtkBorder border;

  border.left = 4;
  border.right = 4;
  border.top = 2;
  border.bottom = 2;

  priv = item->priv = IDO_ENTRY_MENU_ITEM_GET_PRIVATE (item);

  priv->entry = g_object_new (gtk_entry_get_type (),
                              "inner-border", &border,
                              NULL);

  g_signal_connect (priv->entry,
                    "realize",
                    G_CALLBACK (entry_realized_cb),
                    item);
  g_signal_connect (priv->entry,
                    "move-focus",
                    G_CALLBACK (entry_move_focus_cb),
                    item);

  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_box_pack_start (GTK_BOX (priv->box), priv->entry, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (item), priv->box);

  gtk_widget_show_all (priv->box);
}

static gboolean
is_key_press_valid (IdoEntryMenuItem *item,
                    gint              key)
{
  switch (key)
    {
    case GDK_KEY_Escape:
    case GDK_KEY_Up:
    case GDK_KEY_Down:
    case GDK_KEY_KP_Up:
    case GDK_KEY_KP_Down:
      return FALSE;

    default:
      return TRUE;
    }
}

static gboolean
ido_entry_menu_item_key_press (GtkWidget     *widget,
                               GdkEventKey   *event,
                               gpointer       data)
{
  IdoEntryMenuItem *menuitem = (IdoEntryMenuItem *)data;

  if (menuitem->priv->selected &&
      is_key_press_valid (menuitem, event->keyval))
    {
      GtkWidget *entry = menuitem->priv->entry;

      gtk_widget_event (entry,
                        ((GdkEvent *)(void*)(event)));

      /* We've handled the event, but if the key was GDK_KEY_Return
       * we still want to forward the event up to the menu shell
       * to ensure that the menuitem receives the activate signal.
       */
      return event->keyval != GDK_KEY_Return;
    }

  return FALSE;
}

static void
ido_entry_menu_item_send_focus_change (GtkWidget *widget,
                                       gboolean   in)
{
  GdkEvent *event = gdk_event_new (GDK_FOCUS_CHANGE);

  g_object_ref (widget);

  event->focus_change.type = GDK_FOCUS_CHANGE;
  event->focus_change.window = g_object_ref (gtk_widget_get_window (widget));
  event->focus_change.in = in;

  gtk_widget_event (widget, event);

  g_object_notify (G_OBJECT (widget), "has-focus");

  g_object_unref (widget);
  gdk_event_free (event);
}

static gboolean
ido_entry_menu_item_button_press (GtkWidget      *widget,
                                  GdkEventButton *event)
{
  GtkWidget *entry = IDO_ENTRY_MENU_ITEM (widget)->priv->entry;

  if (event->button == 1)
    {
      if (gtk_widget_get_window (entry) != NULL)
        {
          gdk_window_raise (gtk_widget_get_window (entry));
        }

      if (!gtk_widget_has_focus (entry))
        {
          gtk_widget_grab_focus (entry);
        }

      gtk_widget_event (entry,
                        ((GdkEvent *)(void*)(event)));

      return TRUE;
    }

  return FALSE;
}

static gboolean
ido_entry_menu_item_button_release (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  GtkWidget *entry = IDO_ENTRY_MENU_ITEM (widget)->priv->entry;

  gtk_widget_event (entry,
                    ((GdkEvent *)(void*)(event)));

  return TRUE;
}

static void
ido_entry_menu_item_select (GtkMenuItem *item)
{
  IDO_ENTRY_MENU_ITEM (item)->priv->selected = TRUE;

  ido_entry_menu_item_send_focus_change (GTK_WIDGET (IDO_ENTRY_MENU_ITEM (item)->priv->entry), TRUE);
}

static void
ido_entry_menu_item_deselect (GtkMenuItem *item)
{
  IDO_ENTRY_MENU_ITEM (item)->priv->selected = FALSE;

  ido_entry_menu_item_send_focus_change (GTK_WIDGET (IDO_ENTRY_MENU_ITEM (item)->priv->entry), FALSE);
}


static void
entry_realized_cb (GtkWidget        *widget,
                   IdoEntryMenuItem *item)
{
  if (gtk_widget_get_window (widget) != NULL)
    {
      gdk_window_raise (gtk_widget_get_window (widget));
    }

  g_signal_connect (gtk_widget_get_parent (GTK_WIDGET (item)),
                    "key-press-event",
                    G_CALLBACK (ido_entry_menu_item_key_press),
                    item);

  ido_entry_menu_item_send_focus_change (widget, TRUE);
}

static void
entry_move_focus_cb (GtkWidget        *widget,
                     GtkDirectionType  direction,
                     IdoEntryMenuItem *item)
{
  ido_entry_menu_item_send_focus_change (GTK_WIDGET (IDO_ENTRY_MENU_ITEM (item)->priv->entry), FALSE);

  g_signal_emit_by_name (item,
                         "move-focus",
                         GTK_DIR_TAB_FORWARD);
}

/**
 * ido_entry_menu_item_new:
 *
 * Creates a new #IdoEntryMenuItem.
 *
 * Return Value: the newly created #IdoEntryMenuItem.
 */
GtkWidget *
ido_entry_menu_item_new (void)
{
  return g_object_new (IDO_TYPE_ENTRY_MENU_ITEM, NULL);
}

/**
 * ido_entry_menu_item_get_entry:
 * @menuitem: The #IdoEntryMenuItem.
 *
 * Get the #GtkEntry used in this menu item.
 *
 * Return Value: (transfer none): The #GtkEntry inside this menu item.
 */
GtkWidget *
ido_entry_menu_item_get_entry (IdoEntryMenuItem *menuitem)
{
  g_return_val_if_fail (IDO_IS_ENTRY_MENU_ITEM (menuitem), NULL);

  return menuitem->priv->entry;
}
