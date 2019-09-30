/*
 * A GtkCheckMenuItem that uses a GtkSwitch to show its 'active' property.
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

#ifndef __IDO_SWITCH_MENU_ITEM_H__
#define __IDO_SWITCH_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDO_TYPE_SWITCH_MENU_ITEM         (ido_switch_menu_item_get_type ())
#define IDO_SWITCH_MENU_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_SWITCH_MENU_ITEM, IdoSwitchMenuItem))
#define IDO_SWITCH_MENU_ITEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), IDO_TYPE_SWITCH_MENU_ITEM, IdoSwitchMenuItemClass))
#define IDO_IS_SWITCH_MENU_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_SWITCH_MENU_ITEM))
#define IDO_IS_SWITCH_MENU_ITEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), IDO_TYPE_SWITCH_MENU_ITEM))
#define IDO_SWITCH_MENU_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), IDO_TYPE_SWITCH_MENU_ITEM, IdoSwitchMenuItemClass))

typedef struct _IdoSwitchMenuItem        IdoSwitchMenuItem;
typedef struct _IdoSwitchMenuItemClass   IdoSwitchMenuItemClass;
typedef struct _IdoSwitchMenuItemPrivate IdoSwitchMenuItemPrivate;

struct _IdoSwitchMenuItem
{
  GtkCheckMenuItem parent_instance;

  IdoSwitchMenuItemPrivate *priv;
};

struct _IdoSwitchMenuItemClass
{
  GtkCheckMenuItemClass parent_class;
};

GType         ido_switch_menu_item_get_type         (void) G_GNUC_CONST;
GtkWidget    *ido_switch_menu_item_new              (void);
GtkContainer *ido_switch_menu_item_get_content_area (IdoSwitchMenuItem * item);

GtkMenuItem  * ido_switch_menu_item_new_from_menu_model (GMenuItem    *menuitem,
                                                         GActionGroup *actions);

void          ido_switch_menu_item_set_label        (IdoSwitchMenuItem *item,
                                                     const gchar       *label);

void          ido_switch_menu_item_set_icon         (IdoSwitchMenuItem *item,
                                                     GIcon             *icon);

G_END_DECLS

#endif /* __IDO_SWITCH_MENU_ITEM_H__ */
