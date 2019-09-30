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
#include "idoactionhelper.h"
#include "idocalendarmenuitem.h"
#include "config.h"

static void     ido_calendar_menu_item_finalize          (GObject        *item);
static void     ido_calendar_menu_item_select            (GtkMenuItem    *item);
static void     ido_calendar_menu_item_deselect          (GtkMenuItem    *item);
static gboolean ido_calendar_menu_item_button_release    (GtkWidget      *widget,
                                                          GdkEventButton *event);
static gboolean ido_calendar_menu_item_button_press      (GtkWidget      *widget,
                                                          GdkEventButton *event);
static gboolean ido_calendar_menu_item_key_press         (GtkWidget      *widget,
                                                          GdkEventKey    *event,
                                                          gpointer        data);
static void     ido_calendar_menu_item_send_focus_change (GtkWidget      *widget,
                                                          gboolean        in);
static void     calendar_realized_cb                     (GtkWidget        *widget,
                                                          IdoCalendarMenuItem *item);
static void     calendar_move_focus_cb                   (GtkWidget        *widget,
                                                          GtkDirectionType  direction,
                                                          IdoCalendarMenuItem *item);
static void     calendar_month_changed_cb                (GtkWidget *widget, 
                                                          gpointer user_data);                             
static void     calendar_day_selected_double_click_cb    (GtkWidget        *widget, 
                                                          gpointer          user_data);
static void     calendar_day_selected_cb                 (GtkWidget        *widget, 
                                                          gpointer          user_data);                               
struct _IdoCalendarMenuItemPrivate
{
  GtkWidget       *box;
  GtkWidget       *calendar;
  GtkWidget       *parent;
  gboolean         selected;
};

G_DEFINE_TYPE (IdoCalendarMenuItem, ido_calendar_menu_item, GTK_TYPE_MENU_ITEM)

#define IDO_CALENDAR_MENU_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), IDO_TYPE_CALENDAR_MENU_ITEM, IdoCalendarMenuItemPrivate))

static void
ido_calendar_menu_item_class_init (IdoCalendarMenuItemClass *klass)
{
  GObjectClass     *gobject_class;
  GtkWidgetClass   *widget_class;
  GtkMenuItemClass *menu_item_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  menu_item_class = GTK_MENU_ITEM_CLASS (klass);

  gobject_class->finalize = ido_calendar_menu_item_finalize;

  widget_class->button_release_event = ido_calendar_menu_item_button_release;
  widget_class->button_press_event = ido_calendar_menu_item_button_press;

  menu_item_class->select = ido_calendar_menu_item_select;
  menu_item_class->deselect = ido_calendar_menu_item_deselect;

  menu_item_class->hide_on_activate = TRUE;

  g_type_class_add_private (gobject_class, sizeof (IdoCalendarMenuItemPrivate));

  g_signal_new("month-changed", G_TYPE_FROM_CLASS(klass),
                                G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  g_signal_new("day-selected",  G_TYPE_FROM_CLASS(klass),
                                G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_new("day-selected-double-click",  G_TYPE_FROM_CLASS(klass),
                                G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
}

static void
ido_calendar_menu_item_init (IdoCalendarMenuItem *item)
{
  IdoCalendarMenuItemPrivate *priv;

  priv = item->priv = IDO_CALENDAR_MENU_ITEM_GET_PRIVATE (item);

  /* Will be disposed automatically */
  priv->calendar = g_object_new (gtk_calendar_get_type (),
                                 NULL);
  g_object_add_weak_pointer (G_OBJECT (priv->calendar),
                             (gpointer*) &priv->calendar);

  g_signal_connect (priv->calendar,
                    "realize",
                    G_CALLBACK (calendar_realized_cb),
                    item);
  g_signal_connect (priv->calendar,
                    "move-focus",
                    G_CALLBACK (calendar_move_focus_cb),
                    item);

  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_box_pack_start (GTK_BOX (priv->box), priv->calendar, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (item), priv->box);

  gtk_widget_show_all (priv->box);
}

static void
ido_calendar_menu_item_finalize (GObject *object)
{
  IdoCalendarMenuItem *item = IDO_CALENDAR_MENU_ITEM (object);
  IdoCalendarMenuItemPrivate *priv = IDO_CALENDAR_MENU_ITEM_GET_PRIVATE (item);

  if (G_IS_OBJECT (priv->calendar))
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->calendar),
                                    (gpointer*) &priv->calendar);
      g_signal_handlers_disconnect_by_data (priv->calendar, item);
    }

  if (G_IS_OBJECT (priv->parent))
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->parent),
                                    (gpointer*) &priv->parent);
      g_signal_handlers_disconnect_by_data (priv->parent, item);
    }

  G_OBJECT_CLASS (ido_calendar_menu_item_parent_class)->finalize (object);
}

static void
ido_calendar_menu_item_send_focus_change (GtkWidget *widget,
                                          gboolean   in)
{
  GdkEvent *event = gdk_event_new (GDK_FOCUS_CHANGE);

  g_object_ref (widget);

  if (in)
    gtk_widget_grab_focus (widget);

  event->focus_change.type = GDK_FOCUS_CHANGE;
  event->focus_change.window = g_object_ref (gtk_widget_get_window (widget));
  event->focus_change.in = in;

  gtk_widget_event (widget, event);

  g_object_notify (G_OBJECT (widget), "has-focus");

  g_object_unref (widget);
  gdk_event_free (event);
}

static gboolean
ido_calendar_menu_item_key_press (GtkWidget   *widget,
                                  GdkEventKey *event,
                                  gpointer     data)
{
  IdoCalendarMenuItem *menuitem = (IdoCalendarMenuItem *)data;

  g_return_val_if_fail (IDO_IS_CALENDAR_MENU_ITEM (menuitem), FALSE);

  if (menuitem->priv->selected)
    {
      GtkWidget *calendar = menuitem->priv->calendar;

      gtk_widget_event (calendar,
                        ((GdkEvent *)(void*)(event)));

      if (gtk_widget_get_window (calendar) != NULL)
        {
          gdk_window_raise (gtk_widget_get_window (calendar));
        }

      if (!gtk_widget_has_focus (calendar))
        {
          gtk_widget_grab_focus (calendar);
        }

      return (event->keyval != GDK_KEY_Return) &&
             (event->keyval != GDK_KEY_Escape);
    }

  return FALSE;
}

static gboolean
ido_calendar_menu_item_button_press (GtkWidget      *widget,
                                     GdkEventButton *event)
{
	GtkWidget *calendar = IDO_CALENDAR_MENU_ITEM (widget)->priv->calendar;

	if (event->button == 1) {
		if (gtk_widget_get_window (calendar) != NULL) {
			gdk_window_raise (gtk_widget_get_window (calendar));
		}

		if (!gtk_widget_has_focus (calendar)) {
			gtk_widget_grab_focus (calendar);
		}

		GdkEvent * newevent = gdk_event_copy((GdkEvent *)(event));
		GList * children = gdk_window_get_children(gtk_widget_get_window(calendar));
		GList * child;

		gint root_x = event->x_root;
		gint root_y = event->y_root;

		for (child = children; child != NULL; child = g_list_next(child)) {
			gint newx, newy;
			gint winx, winy;
			GdkWindow * newwindow = (GdkWindow*)child->data;

			((GdkEventButton *)newevent)->window = newwindow;

			gdk_window_get_origin(newwindow, &winx, &winy);
			newx = root_x - winx;
			newy = root_y - winy;

			if (newx >= 0 && newy >= 0 && newx < gdk_window_get_width(newwindow) && newy < gdk_window_get_height(newwindow)) {
				((GdkEventButton *)newevent)->x = newx;
				((GdkEventButton *)newevent)->y = newy;

				GTK_WIDGET_GET_CLASS(calendar)->button_press_event(GTK_WIDGET(calendar), (GdkEventButton*)newevent);
			}
		}

		((GdkEventButton *)newevent)->window = event->window;
		gdk_event_free(newevent);

		return TRUE;
	}

	return FALSE;
}

static gboolean
ido_calendar_menu_item_button_release (GtkWidget      *widget,
                                       GdkEventButton *event)
{
  GtkWidget *calendar = IDO_CALENDAR_MENU_ITEM (widget)->priv->calendar;
  GTK_WIDGET_GET_CLASS(calendar)->button_release_event(GTK_WIDGET(calendar), event);

  return TRUE;
}

static void
ido_calendar_menu_item_select (GtkMenuItem *item)
{
  IDO_CALENDAR_MENU_ITEM (item)->priv->selected = TRUE;

  ido_calendar_menu_item_send_focus_change (GTK_WIDGET (IDO_CALENDAR_MENU_ITEM (item)->priv->calendar), TRUE);
}

static void
ido_calendar_menu_item_deselect (GtkMenuItem *item)
{
  IDO_CALENDAR_MENU_ITEM (item)->priv->selected = FALSE;

  ido_calendar_menu_item_send_focus_change (GTK_WIDGET (IDO_CALENDAR_MENU_ITEM (item)->priv->calendar), FALSE);
}

static void
calendar_realized_cb (GtkWidget        *widget,
                      IdoCalendarMenuItem *item)
{
  if (gtk_widget_get_window (widget) != NULL)
    {
      gdk_window_raise (gtk_widget_get_window (widget));
    }

  item->priv->parent = gtk_widget_get_parent (GTK_WIDGET (item));

  g_object_add_weak_pointer (G_OBJECT (item->priv->parent),
                             (gpointer*) &item->priv->parent);

  g_signal_connect (item->priv->parent,
                    "key-press-event",
                    G_CALLBACK (ido_calendar_menu_item_key_press),
                    item);

  g_signal_connect (item->priv->calendar,
                    "month-changed",
                    G_CALLBACK (calendar_month_changed_cb),
                    item);
  g_signal_connect (item->priv->calendar,
                    "day-selected",
                    G_CALLBACK (calendar_day_selected_cb),
                    item);
  g_signal_connect (item->priv->calendar,
                    "day-selected-double-click",
                    G_CALLBACK (calendar_day_selected_double_click_cb),
                    item);

  ido_calendar_menu_item_send_focus_change (widget, TRUE);
}

static void
calendar_move_focus_cb (GtkWidget        *widget,
                        GtkDirectionType  direction,
                        IdoCalendarMenuItem *item)
{
  ido_calendar_menu_item_send_focus_change (GTK_WIDGET (IDO_CALENDAR_MENU_ITEM (item)->priv->calendar), FALSE);

  g_signal_emit_by_name (item,
                         "move-focus",
                         GTK_DIR_TAB_FORWARD);
}

static void
calendar_month_changed_cb (GtkWidget        *widget, 
                           gpointer          user_data)
{
  IdoCalendarMenuItem *item = (IdoCalendarMenuItem *)user_data;
  g_signal_emit_by_name (item, "month-changed", NULL);
}

static void
calendar_day_selected_cb (GtkWidget        *widget, 
                          gpointer          user_data)
{
  IdoCalendarMenuItem *item = (IdoCalendarMenuItem *)user_data;
  g_signal_emit_by_name (item, "day-selected", NULL);
}

static void
calendar_day_selected_double_click_cb (GtkWidget        *widget, 
                                       gpointer          user_data)
{
  IdoCalendarMenuItem *item = (IdoCalendarMenuItem *)user_data;
  guint day, month, year;
  gtk_calendar_get_date (GTK_CALENDAR (widget), &year, &month, &day);
  g_signal_emit_by_name (item, "day-selected-double-click", NULL);
}

/**
 * ido_calendar_menu_item_new:
 *
 * Creates a new #IdoCalendarMenuItem
 *
 * Return Value: a new #IdoCalendarMenuItem.
 **/
GtkWidget *
ido_calendar_menu_item_new (void)
{
  return g_object_new (IDO_TYPE_CALENDAR_MENU_ITEM, NULL);
}

/**
 * ido_calendar_menu_item_get_calendar:
 * @menuitem: A #IdoCalendarMenuItem
 *
 * Returns the calendar associated with this menu item.
 *
 * Return Value: (transfer none): The #GtkCalendar used in this item.
 */
GtkWidget *
ido_calendar_menu_item_get_calendar (IdoCalendarMenuItem *menuitem)
{
  g_return_val_if_fail (IDO_IS_CALENDAR_MENU_ITEM (menuitem), NULL);

  return menuitem->priv->calendar;
}

/**
 * ido_calendar_menu_item_mark_day:
 * @menuitem: A #IdoCalendarMenuItem
 * @day: the day number to unmark between 1 and 31.
 *
 * Places a visual marker on a particular day. 
 *
 * Return Value: #TRUE
 */
gboolean
ido_calendar_menu_item_mark_day	(IdoCalendarMenuItem *menuitem, guint day)
{
  g_return_val_if_fail(IDO_IS_CALENDAR_MENU_ITEM(menuitem), FALSE);
  
  gtk_calendar_mark_day(GTK_CALENDAR (menuitem->priv->calendar), day);
  return TRUE;
}

/**
 * ido_calendar_menu_item_unmark_day:
 * @menuitem: A #IdoCalendarMenuItem
 * @day: the day number to unmark between 1 and 31.
 * 
 * Removes the visual marker from a particular day.
 *
 * Return Value: #TRUE
 */
gboolean
ido_calendar_menu_item_unmark_day (IdoCalendarMenuItem *menuitem, guint day)
{
  g_return_val_if_fail(IDO_IS_CALENDAR_MENU_ITEM(menuitem), FALSE);
  
  gtk_calendar_unmark_day(GTK_CALENDAR (menuitem->priv->calendar), day);
  return TRUE;
}

/**
 * ido_calendar_menu_item_clear_marks:
 * @menuitem: A #IdoCalendarMenuItem
 *
 * Remove all visual markers. 
 */
void
ido_calendar_menu_item_clear_marks (IdoCalendarMenuItem *menuitem)
{
  g_return_if_fail(IDO_IS_CALENDAR_MENU_ITEM(menuitem));
  
  gtk_calendar_clear_marks(GTK_CALENDAR (menuitem->priv->calendar));
}

/**
 * ido_calendar_menu_item_set_display_options:
 * @menuitem: A #IdoCalendarMenuItem
 * @flags: the display options to set
 *
 * Set the display options for the calendar.
 */
void
ido_calendar_menu_item_set_display_options (IdoCalendarMenuItem *menuitem, GtkCalendarDisplayOptions flags)
{
  g_return_if_fail(IDO_IS_CALENDAR_MENU_ITEM(menuitem));
  
  gtk_calendar_set_display_options (GTK_CALENDAR (menuitem->priv->calendar), flags);
}

/**
 * ido_calendar_menu_item_get_display_options:
 * @menuitem: A #IdoCalendarMenuItem
 *
 * Get the display options for the calendar.
 *
 * Return Value: the display options in use
 */
GtkCalendarDisplayOptions
ido_calendar_menu_item_get_display_options (IdoCalendarMenuItem *menuitem)
{
  g_return_val_if_fail(IDO_IS_CALENDAR_MENU_ITEM(menuitem), 0);
  
  return gtk_calendar_get_display_options (GTK_CALENDAR (menuitem->priv->calendar));
}

/**
 * ido_calendar_menu_item_get_date:
 * @menuitem: A #IdoCalendarMenuItem
 * @year: (out) (allow-none): location to store the year as a decimal number (e.g. 2011), or #NULL.
 * @month: (out) (allow-none): location to store the month number (between 0 and 11), or #NULL.
 * @day: (out) (allow-none): location to store the day number (between 1 and 31), or #NULL.
 *
 * Gets the selected date.
 */
void 
ido_calendar_menu_item_get_date (IdoCalendarMenuItem *menuitem,
                                 guint *year,
                                 guint *month,
                                 guint *day) {
	
  g_return_if_fail(IDO_IS_CALENDAR_MENU_ITEM(menuitem));
  gtk_calendar_get_date (GTK_CALENDAR (menuitem->priv->calendar), year, month, day);
}

/**
 * ido_calendar_menu_item_set_date:
 * @menuitem: A #IdoCalendarMenuItem
 * @year: the year to show (e.g. 2011).
 * @month: a month number (between 0 and 11).
 * @day: The day number (between 1 and 31).
 *
 * Set the date shown on the calendar.
 *
 * Return Value: #TRUE
 */
gboolean
ido_calendar_menu_item_set_date (IdoCalendarMenuItem *menuitem,
                                 guint year,
                                 guint month,
                                 guint day)
{
  guint old_y, old_m, old_d;

  g_return_val_if_fail (IDO_IS_CALENDAR_MENU_ITEM(menuitem), FALSE);

  ido_calendar_menu_item_get_date (menuitem, &old_y, &old_m, &old_d);

  if ((old_y != year) || (old_m != month))
    gtk_calendar_select_month (GTK_CALENDAR (menuitem->priv->calendar), month, year);

  if (old_d != day)
    gtk_calendar_select_day (GTK_CALENDAR (menuitem->priv->calendar), day);

  return TRUE;
}

/***
****
****
****
***/

static void
activate_current_day (IdoCalendarMenuItem * ido_calendar,
                      const char          * action_name_key)
{
  GObject * o;
  const char * action_name;
  GActionGroup * action_group;

  o = G_OBJECT (ido_calendar);
  action_name = g_object_get_data (o, action_name_key);
  action_group = g_object_get_data (o, "ido-action-group");

  if (action_group && action_name)
    {
      guint y, m, d;
      GDateTime * date_time;
      GVariant * target;

      ido_calendar_menu_item_get_date (ido_calendar, &y, &m, &d);
      m++; /* adjust month from GtkCalendar (0 based) to GDateTime (1 based) */
      date_time = g_date_time_new_local (y, m, d, 9, 0, 0);
      target = g_variant_new_int64 (g_date_time_to_unix (date_time));
  
      g_action_group_activate_action (action_group, action_name, target);

      g_date_time_unref (date_time);
    }
}

static void
on_day_selected (IdoCalendarMenuItem * ido_calendar)
{
  activate_current_day (ido_calendar, "ido-selection-action-name");
}

static void
on_day_double_clicked (IdoCalendarMenuItem * ido_calendar)
{
  activate_current_day (ido_calendar, "ido-activation-action-name");
}

static void
on_action_state_changed (IdoActionHelper * helper,
                         GVariant        * state,
                         gpointer          unused G_GNUC_UNUSED)
{
  GVariant * v;
  const char * key;
  IdoCalendarMenuItem * ido_calendar;

  ido_calendar = IDO_CALENDAR_MENU_ITEM (ido_action_helper_get_widget (helper));

  g_return_if_fail (ido_calendar != NULL);
  g_return_if_fail (g_variant_is_of_type (state, G_VARIANT_TYPE_DICTIONARY));

  /* an int64 representing a time_t indicating which year and month should
     be visible in the calendar and which day should be given the cursor. */
  key = "calendar-day";
  if ((v = g_variant_lookup_value (state, key, G_VARIANT_TYPE_INT64)))
    {
      int y, m, d;
      time_t t;
      GDateTime * date_time;

      t = g_variant_get_int64 (v);
      date_time = g_date_time_new_from_unix_local (t);
      g_date_time_get_ymd (date_time, &y, &m, &d);
      m--; /* adjust month from GDateTime (1 based) to GtkCalendar (0 based) */
      ido_calendar_menu_item_set_date (ido_calendar, y, m, d);

      g_date_time_unref (date_time);
      g_variant_unref (v);
    }

  /* a boolean value of whether or not to show the week numbers */
  key = "show-week-numbers";
  if ((v = g_variant_lookup_value (state, key, G_VARIANT_TYPE_BOOLEAN)))
    {
      const GtkCalendarDisplayOptions old_flags = ido_calendar_menu_item_get_display_options (ido_calendar);
      GtkCalendarDisplayOptions new_flags = old_flags;

      if (g_variant_get_boolean (v))
        new_flags |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
      else
        new_flags &= ~GTK_CALENDAR_SHOW_WEEK_NUMBERS;

      if (new_flags != old_flags)
        ido_calendar_menu_item_set_display_options (ido_calendar, new_flags);

      g_variant_unref (v);
    }

  /* an array of int32 day-of-months denoting days that have appointments */
  key = "appointment-days";
  ido_calendar_menu_item_clear_marks (ido_calendar);
  if ((v = g_variant_lookup_value (state, key, G_VARIANT_TYPE("ai"))))
    {
      gint32 day;
      GVariantIter iter;

      g_variant_iter_init (&iter, v);
      while (g_variant_iter_next (&iter, "i", &day))
        ido_calendar_menu_item_mark_day (ido_calendar, day);

      g_variant_unref (v);
    }
}

GtkMenuItem *
ido_calendar_menu_item_new_from_model (GMenuItem    * menu_item,
                                       GActionGroup * actions)
{
  GObject * o;
  GtkWidget * calendar;
  IdoCalendarMenuItem * ido_calendar;
  gchar * selection_action_name = NULL;
  gchar * activation_action_name = NULL;

  /* get the select & activate action names */
  g_menu_item_get_attribute (menu_item, "action", "s", &selection_action_name);
  g_menu_item_get_attribute (menu_item, "activation-action", "s", &activation_action_name);

  /* remember the action group & action names so that we can poke them
     when user selects and double-clicks */
  ido_calendar = IDO_CALENDAR_MENU_ITEM (ido_calendar_menu_item_new ());
  o = G_OBJECT (ido_calendar);
  g_object_set_data_full (o, "ido-action-group", g_object_ref(actions), g_object_unref);
  g_object_set_data_full (o, "ido-selection-action-name", selection_action_name, g_free);
  g_object_set_data_full (o, "ido-activation-action-name", activation_action_name, g_free);
  calendar = ido_calendar_menu_item_get_calendar (ido_calendar);
  g_signal_connect_swapped (calendar, "day-selected",
                            G_CALLBACK(on_day_selected), ido_calendar);
  g_signal_connect_swapped (calendar, "day-selected-double-click",
                            G_CALLBACK(on_day_double_clicked), ido_calendar);

  /* Use an IdoActionHelper for state updates.
     Since we have two separate actions for selection & activation,
     we'll do the activation & targets logic here in ido-calendar */
  if (selection_action_name != NULL)
    {
      IdoActionHelper * helper;

      helper = ido_action_helper_new (GTK_WIDGET(ido_calendar),
                                      actions,
                                      selection_action_name,
                                      NULL);
      g_signal_connect (helper, "action-state-changed",
                        G_CALLBACK (on_action_state_changed), NULL);
      g_signal_connect_swapped (ido_calendar, "destroy",
                                G_CALLBACK (g_object_unref), helper);
    }

  return GTK_MENU_ITEM (ido_calendar);
}
