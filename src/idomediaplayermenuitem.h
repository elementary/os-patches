/*
 * Copyright 2013 Canonical Ltd.
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
 *
 * Authors:
 *     Conor Curran <conor.curran@canonical.com>
 *     Mirco Müller <mirco.mueller@canonical.com>
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#ifndef __IDO_MEDIA_PLAYER_MENU_ITEM_H__
#define __IDO_MEDIA_PLAYER_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDO_TYPE_MEDIA_PLAYER_MENU_ITEM  (ido_media_player_menu_item_get_type ())
#define IDO_MEDIA_PLAYER_MENU_ITEM(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_MEDIA_PLAYER_MENU_ITEM, IdoMediaPlayerMenuItem))
#define IDO_IS_MEDIA_PLAYER_MENU_ITEM(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_MEDIA_PLAYER_MENU_ITEM))

typedef struct _IdoMediaPlayerMenuItem IdoMediaPlayerMenuItem;

GType                   ido_media_player_menu_item_get_type             (void);

GtkMenuItem *           ido_media_player_menu_item_new_from_model       (GMenuItem    *menuitem,
                                                                         GActionGroup *actions);

G_END_DECLS

#endif
