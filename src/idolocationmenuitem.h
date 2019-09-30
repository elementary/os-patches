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

#ifndef __IDO_LOCATION_MENU_ITEM_H__
#define __IDO_LOCATION_MENU_ITEM_H__

#include <gtk/gtk.h>
#include "idotimestampmenuitem.h"

G_BEGIN_DECLS

#define IDO_LOCATION_MENU_ITEM_TYPE    (ido_location_menu_item_get_type ())
#define IDO_LOCATION_MENU_ITEM(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDO_LOCATION_MENU_ITEM_TYPE, IdoLocationMenuItem))
#define IDO_IS_LOCATION_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDO_LOCATION_MENU_ITEM_TYPE))

typedef struct _IdoLocationMenuItem        IdoLocationMenuItem;
typedef struct _IdoLocationMenuItemClass   IdoLocationMenuItemClass;
typedef struct _IdoLocationMenuItemPrivate IdoLocationMenuItemPrivate;

struct _IdoLocationMenuItemClass
{
  IdoTimeStampMenuItemClass parent_class;
};

/**
 * A menuitem that indicates a location.
 *
 * It contains a primary label giving the location's name and a
 * right-aligned secondary label showing the location's current time
 */
struct _IdoLocationMenuItem
{
  /*< private >*/
  IdoTimeStampMenuItem parent;
  IdoLocationMenuItemPrivate * priv;
};


GType ido_location_menu_item_get_type (void) G_GNUC_CONST;

GtkWidget * ido_location_menu_item_new (void);

GtkMenuItem * ido_location_menu_item_new_from_model (GMenuItem    * menuitem,
                                                     GActionGroup * actions);

void ido_location_menu_item_set_timezone (IdoLocationMenuItem * menuitem,
                                          const char          * timezone);

void ido_location_menu_item_set_format (IdoLocationMenuItem * menuitem,
                                        const char          * strftime_fmt);


G_END_DECLS

#endif
