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
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#ifndef __IM_DESKTOP_MENU_H__
#define __IM_DESKTOP_MENU_H__

#include "im-menu.h"

#define IM_TYPE_DESKTOP_MENU            (im_desktop_menu_get_type ())
#define IM_DESKTOP_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IM_TYPE_DESKTOP_MENU, ImDesktopMenu))
#define IM_IS_DESKTOP_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IM_TYPE_DESKTOP_MENU))

typedef struct _ImDesktopMenu ImDesktopMenu;

GType               im_desktop_menu_get_type              (void);

ImDesktopMenu *     im_desktop_menu_new                   (ImApplicationList  *applist);

#endif
