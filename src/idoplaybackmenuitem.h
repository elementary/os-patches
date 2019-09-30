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
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#ifndef __IDO_PLAYBACK_MENU_ITEM_H__
#define __IDO_PLAYBACK_MENU_ITEM_H__

#include <gtk/gtk.h>

#define IDO_TYPE_PLAYBACK_MENU_ITEM    (ido_playback_menu_item_get_type ())
#define IDO_PLAYBACK_MENU_ITEM(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDO_TYPE_PLAYBACK_MENU_ITEM, IdoPlaybackMenuItem))
#define IDO_IS_PLAYBACK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDO_TYPE_PLAYBACK_MENU_ITEM))

typedef struct _IdoPlaybackMenuItem      IdoPlaybackMenuItem;

GType                   ido_playback_menu_item_get_type                 (void);

GtkMenuItem *           ido_playback_menu_item_new_from_model           (GMenuItem    *item,
                                                                         GActionGroup *actions);

#endif
