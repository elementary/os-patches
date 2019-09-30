/*
Copyright 2011 Canonical Ltd.

Authors:
    Conor Curran <conor.curran@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __IDO_USER_MENU_ITEM_H__
#define __IDO_USER_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDO_USER_MENU_ITEM_TYPE            (ido_user_menu_item_get_type ())
#define IDO_USER_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDO_USER_MENU_ITEM_TYPE, IdoUserMenuItem))
#define IDO_USER_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), IDO_USER_MENU_ITEM_TYPE, IdoUserMenuItemClass))
#define IS_IDO_USER_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDO_USER_MENU_ITEM_TYPE))
#define IS_IDO_USER_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IDO_USER_MENU_ITEM_TYPE))
#define IDO_USER_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), IDO_USER_MENU_ITEM_TYPE, IdoUserMenuItemClass))

typedef struct _IdoUserMenuItem        IdoUserMenuItem;
typedef struct _IdoUserMenuItemClass   IdoUserMenuItemClass;
typedef struct _IdoUserMenuItemPrivate IdoUserMenuItemPrivate;

/* property keys */
#define IDO_USER_MENU_ITEM_PROP_LABEL           "label"
#define IDO_USER_MENU_ITEM_PROP_ICON_FILENAME   "icon-filename"
#define IDO_USER_MENU_ITEM_PROP_IS_LOGGED_IN    "is-logged-in"
#define IDO_USER_MENU_ITEM_PROP_IS_CURRENT_USER "is-current-user"

struct _IdoUserMenuItemClass
{
  GtkMenuItemClass parent_class;
};

struct _IdoUserMenuItem
{
  /*< private >*/
  GtkMenuItem parent;
  IdoUserMenuItemPrivate * priv;
};

GType ido_user_menu_item_get_type (void) G_GNUC_CONST;

GtkWidget* ido_user_menu_item_new(void);

void ido_user_menu_item_set_icon           (IdoUserMenuItem * self, GIcon      * icon);
void ido_user_menu_item_set_icon_from_file (IdoUserMenuItem * self, const char * filename);
void ido_user_menu_item_set_logged_in      (IdoUserMenuItem * self, gboolean     is_logged_in);
void ido_user_menu_item_set_current_user   (IdoUserMenuItem * self, gboolean     is_current_user);
void ido_user_menu_item_set_label          (IdoUserMenuItem * self, const char * label);

GtkMenuItem * ido_user_menu_item_new_from_model (GMenuItem    *menuitem,
                                                 GActionGroup *actions);

GtkMenuItem * ido_guest_menu_item_new_from_model (GMenuItem    *menuitem,
                                                  GActionGroup *actions);

G_END_DECLS

#endif
