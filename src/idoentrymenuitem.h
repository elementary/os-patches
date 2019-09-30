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

#ifndef __IDO_ENTRY_MENU_ITEM_H__
#define __IDO_ENTRY_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDO_TYPE_ENTRY_MENU_ITEM         (ido_entry_menu_item_get_type ())
#define IDO_ENTRY_MENU_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_ENTRY_MENU_ITEM, IdoEntryMenuItem))
#define IDO_ENTRY_MENU_ITEM_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), IDO_TYPE_ENTRY_MENU_ITEM, IdoEntryMenuItemClass))
#define IDO_IS_ENTRY_MENU_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_ENTRY_MENU_ITEM))
#define IDO_IS_ENTRY_MENU_ITEM_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), IDO_TYPE_ENTRY_MENU_ITEM))
#define IDO_ENTRY_MENU_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), IDO_TYPE_ENTRY_MENU_ITEM, IdoEntryMenuItemClass))

typedef struct _IdoEntryMenuItem        IdoEntryMenuItem;
typedef struct _IdoEntryMenuItemClass   IdoEntryMenuItemClass;
typedef struct _IdoEntryMenuItemPrivate IdoEntryMenuItemPrivate;

struct _IdoEntryMenuItem
{
  GtkMenuItem parent_instance;

  IdoEntryMenuItemPrivate *priv;
};

struct _IdoEntryMenuItemClass
{
  GtkMenuItemClass parent_class;
};

GType      ido_entry_menu_item_get_type      (void) G_GNUC_CONST;

GtkWidget *ido_entry_menu_item_new           (void);
GtkWidget *ido_entry_menu_item_get_entry     (IdoEntryMenuItem *menuitem);

G_END_DECLS

#endif /* __IDO_ENTRY_MENU_ITEM_H__ */
