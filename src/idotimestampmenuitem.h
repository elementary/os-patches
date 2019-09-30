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

#ifndef __IDO_TIME_STAMP_MENU_ITEM_H__
#define __IDO_TIME_STAMP_MENU_ITEM_H__

#include <gtk/gtk.h>
#include "idobasicmenuitem.h"

G_BEGIN_DECLS

#define IDO_TYPE_TIME_STAMP_MENU_ITEM    (ido_time_stamp_menu_item_get_type ())
#define IDO_TIME_STAMP_MENU_ITEM(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDO_TYPE_TIME_STAMP_MENU_ITEM, IdoTimeStampMenuItem))
#define IDO_IS_TIME_STAMP_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDO_TYPE_TIME_STAMP_MENU_ITEM))

typedef struct _IdoTimeStampMenuItem        IdoTimeStampMenuItem;
typedef struct _IdoTimeStampMenuItemClass   IdoTimeStampMenuItemClass;
typedef struct _IdoTimeStampMenuItemPrivate IdoTimeStampMenuItemPrivate;

struct _IdoTimeStampMenuItemClass
{
  IdoBasicMenuItemClass parent_class;
};

/**
 * A menuitem that contains a left-aligned optional icon and label
 * and a right-aligned secondary label showing the specified time
 * in the specified format.
 *
 * Used by IdoLocationMenuItem, IdoAppointmentMenuItem, and IdoAlarmMenuItem.
 */
struct _IdoTimeStampMenuItem
{
  /*< private >*/
  IdoBasicMenuItem parent;
  IdoTimeStampMenuItemPrivate * priv;
};


GType        ido_time_stamp_menu_item_get_type      (void) G_GNUC_CONST;

GtkWidget  * ido_time_stamp_menu_item_new           (void);

void         ido_time_stamp_menu_item_set_date_time (IdoTimeStampMenuItem * menuitem,
                                                     GDateTime            * date_time);

void         ido_time_stamp_menu_item_set_format    (IdoTimeStampMenuItem * menuitem,
                                                     const char           * strftime_fmt);

const char * ido_time_stamp_menu_item_get_format    (IdoTimeStampMenuItem * menuitem);



G_END_DECLS

#endif
