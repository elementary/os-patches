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

#ifndef __IDO_BASIC_MENU_ITEM_H__
#define __IDO_BASIC_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDO_TYPE_BASIC_MENU_ITEM    (ido_basic_menu_item_get_type ())
#define IDO_BASIC_MENU_ITEM(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDO_TYPE_BASIC_MENU_ITEM, IdoBasicMenuItem))
#define IDO_IS_BASIC_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDO_TYPE_BASIC_MENU_ITEM))

typedef struct _IdoBasicMenuItem        IdoBasicMenuItem;
typedef struct _IdoBasicMenuItemClass   IdoBasicMenuItemClass;
typedef struct _IdoBasicMenuItemPrivate IdoBasicMenuItemPrivate;

struct _IdoBasicMenuItemClass
{
  GtkMenuItemClass parent_class;
};

/**
 * A simple menuitem that includes a right-justified secondary text.
 */
struct _IdoBasicMenuItem
{
  /*< private >*/
  GtkMenuItem parent;
  IdoBasicMenuItemPrivate * priv;
};


GType ido_basic_menu_item_get_type               (void) G_GNUC_CONST;

GtkWidget * ido_basic_menu_item_new              (void);

void ido_basic_menu_item_set_icon                (IdoBasicMenuItem * self,
                                                  GIcon            * icon);

void ido_basic_menu_item_set_icon_from_file      (IdoBasicMenuItem * self,
                                                  const char       * filename);

void ido_basic_menu_item_set_text                (IdoBasicMenuItem * self,
                                                  const char       * text);

void ido_basic_menu_item_set_secondary_text      (IdoBasicMenuItem * self,
                                                  const char       * text);

GtkMenuItem * ido_basic_menu_item_new_from_model (GMenuItem    * menuitem,
                                                  GActionGroup * actions);

G_END_DECLS

#endif
