/*
 * Copyright 2012 Canonical Ltd.
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

#ifndef __IM_PHONE_MENU_H__
#define __IM_PHONE_MENU_H__

#include "im-menu.h"

#define IM_TYPE_PHONE_MENU            (im_phone_menu_get_type ())
#define IM_PHONE_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IM_TYPE_PHONE_MENU, ImPhoneMenu))
#define IM_PHONE_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), IM_TYPE_PHONE_MENU, ImPhoneMenuClass))
#define IM_IS_PHONE_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IM_TYPE_PHONE_MENU))
#define IM_IS_PHONE_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IM_TYPE_PHONE_MENU))
#define IM_PHONE_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), IM_TYPE_PHONE_MENU, ImPhoneMenuClass))

typedef struct _ImPhoneMenu ImPhoneMenu;

GType               im_phone_menu_get_type              (void);

ImPhoneMenu *       im_phone_menu_new                   (ImApplicationList  *applist);

void                im_phone_menu_add_message           (ImPhoneMenu        *menu,
                                                         const gchar        *app_id,
                                                         GIcon              *app_icon,
                                                         const gchar        *id,
                                                         GVariant           *serialized_icon,
                                                         const gchar        *title,
                                                         const gchar        *subtitle,
                                                         const gchar        *body,
                                                         GVariant           *actions,
                                                         gint64              time);

void                im_phone_menu_remove_message        (ImPhoneMenu        *menu,
                                                         const gchar        *app_id,
                                                         const gchar        *id);

void                im_phone_menu_add_source            (ImPhoneMenu        *menu,
                                                         const gchar        *app_id,
                                                         const gchar        *id,
                                                         const gchar        *label,
                                                         const gchar        *iconstr);

void                im_phone_menu_remove_source         (ImPhoneMenu        *menu,
                                                         const gchar        *app_id,
                                                         const gchar        *id);

void                im_phone_menu_remove_application    (ImPhoneMenu        *menu,
                                                         const gchar        *app_id);

void                im_phone_menu_remove_all            (ImPhoneMenu        *menu);

#endif
