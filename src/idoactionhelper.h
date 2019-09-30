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

#ifndef __IDO_ACTION_HELPER_H__
#define __IDO_ACTION_HELPER_H__

#include <gtk/gtk.h>

#define IDO_TYPE_ACTION_HELPER  (ido_action_helper_get_type ())
#define IDO_ACTION_HELPER(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_ACTION_HELPER, IdoActionHelper))
#define IDO_IS_ACTION_HELPER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_ACTION_HELPER))

typedef struct _IdoActionHelper IdoActionHelper;

GType               ido_menu_item_get_type              (void);

IdoActionHelper *   ido_action_helper_new               (GtkWidget    *widget,
                                                         GActionGroup *action_group,
                                                         const gchar  *action_name,
                                                         GVariant     *target);

GtkWidget *         ido_action_helper_get_widget        (IdoActionHelper *helper);

GVariant *          ido_action_helper_get_action_target (IdoActionHelper *helper);

void                ido_action_helper_activate          (IdoActionHelper *helper);

void                ido_action_helper_activate_with_parameter (IdoActionHelper *helper,
                                                               GVariant        *parameter);

void                ido_action_helper_change_action_state (IdoActionHelper *helper,
                                                           GVariant        *state);

#endif
