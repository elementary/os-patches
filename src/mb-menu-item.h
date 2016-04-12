/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ap-menu-item.h - Class to represent a Wifi access point 
 *
 * Jonathan Blandford <jrb@redhat.com>
 * Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2005 - 2010 Red Hat, Inc.
 */

#ifndef _MB_MENU_ITEM_H_
#define _MB_MENU_ITEM_H_

#include <gtk/gtk.h>
#include "applet.h"
#include "mobile-helpers.h"

#define NM_TYPE_MB_MENU_ITEM            (nm_mb_menu_item_get_type ())
#define NM_MB_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_MB_MENU_ITEM, NMMbMenuItem))
#define NM_MB_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NM_TYPE_MB_MENU_ITEM, NMMbMenuItemClass))
#define NM_IS_MB_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_MB_MENU_ITEM))
#define NM_IS_MB_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NM_TYPE_MB_MENU_ITEM))
#define NM_MB_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NM_TYPE_MB_MENU_ITEM, NMMbMenuItemClass))

typedef struct {
	GtkMenuItem image_item;
} NMMbMenuItem;

typedef struct {
	GtkMenuItemClass parent_class;
} NMMbMenuItemClass;


GType	   nm_mb_menu_item_get_type (void) G_GNUC_CONST;

GtkWidget *nm_mb_menu_item_new (const char *connection_name,
                                guint32 strength,
                                const char *provider,
                                gboolean active,
                                guint32 technology,
                                guint32 state,
                                gboolean enabled,
                                NMApplet *applet);

#endif /* _MB_MENU_ITEM_H_ */

