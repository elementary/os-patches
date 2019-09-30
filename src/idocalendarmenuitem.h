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

#ifndef __IDO_CALENDAR_MENU_ITEM_H__
#define __IDO_CALENDAR_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDO_TYPE_CALENDAR_MENU_ITEM         (ido_calendar_menu_item_get_type ())
#define IDO_CALENDAR_MENU_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_CALENDAR_MENU_ITEM, IdoCalendarMenuItem))
#define IDO_CALENDAR_MENU_ITEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), IDO_TYPE_CALENDAR_MENU_ITEM, IdoCalendarMenuItemClass))
#define IDO_IS_CALENDAR_MENU_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_CALENDAR_MENU_ITEM))
#define IDO_IS_CALENDAR_MENU_ITEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), IDO_TYPE_CALENDAR_MENU_ITEM))
#define IDO_CALENDAR_MENU_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), IDO_TYPE_CALENDAR_MENU_ITEM, IdoCalendarMenuItemClass))

typedef struct _IdoCalendarMenuItem        IdoCalendarMenuItem;
typedef struct _IdoCalendarMenuItemClass   IdoCalendarMenuItemClass;
typedef struct _IdoCalendarMenuItemPrivate IdoCalendarMenuItemPrivate;

struct _IdoCalendarMenuItem
{
  GtkMenuItem parent_instance;

  IdoCalendarMenuItemPrivate *priv;
};

struct _IdoCalendarMenuItemClass
{
  GtkMenuItemClass parent_class;
};

GType      ido_calendar_menu_item_get_type            (void) G_GNUC_CONST;

GtkWidget *ido_calendar_menu_item_new                 (void);
GtkWidget *ido_calendar_menu_item_get_calendar  (IdoCalendarMenuItem *menuitem);
gboolean   ido_calendar_menu_item_mark_day            (IdoCalendarMenuItem *menuitem, guint day);
gboolean   ido_calendar_menu_item_unmark_day          (IdoCalendarMenuItem *menuitem, guint day);
void       ido_calendar_menu_item_clear_marks         (IdoCalendarMenuItem *menuitem);
void       ido_calendar_menu_item_set_display_options (IdoCalendarMenuItem *menuitem, GtkCalendarDisplayOptions flags);
GtkCalendarDisplayOptions ido_calendar_menu_item_get_display_options (IdoCalendarMenuItem *menuitem);
void       ido_calendar_menu_item_get_date            (IdoCalendarMenuItem *menuitem,
                                                       guint               *year,
                                                       guint               *month,
                                                       guint               *day);
gboolean   ido_calendar_menu_item_set_date            (IdoCalendarMenuItem *menuitem,
                                                       guint year,
                                                       guint month,
                                                       guint day);

GtkMenuItem * ido_calendar_menu_item_new_from_model   (GMenuItem    * menuitem,
                                                       GActionGroup * actions);


G_END_DECLS

#endif /* __IDO_CALENDAR_MENU_ITEM_H__ */
