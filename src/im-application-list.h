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

#ifndef __IM_APPLICATION_LIST_H__
#define __IM_APPLICATION_LIST_H__

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#define IM_TYPE_APPLICATION_LIST            (im_application_list_get_type ())
#define IM_APPLICATION_LIST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IM_TYPE_APPLICATION_LIST, ImApplicationList))
#define IM_APPLICATION_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), IM_TYPE_APPLICATION_LIST, ImApplicationListClass))
#define IM_IS_APPLICATION_LIST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IM_TYPE_APPLICATION_LIST))
#define IM_IS_APPLICATION_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IM_TYPE_APPLICATION_LIST))
#define IM_APPLICATION_LIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), IM_TYPE_APPLICATION_LIST, ImApplicationListClass))

typedef struct _ImApplicationList        ImApplicationList;

GType                   im_application_list_get_type            (void);

ImApplicationList *     im_application_list_new                 (void);

gboolean                im_application_list_add                 (ImApplicationList *list,
                                                                 const gchar       *desktop_id);

void                    im_application_list_remove              (ImApplicationList *list,
                                                                 const gchar       *id);

void                    im_application_list_set_remote          (ImApplicationList *list,
                                                                 const gchar       *id,
                                                                 GDBusConnection   *connection,
                                                                 const gchar       *unique_bus_name,
                                                                 const gchar       *object_path);

GActionGroup *          im_application_list_get_action_group    (ImApplicationList *list);

GList *                 im_application_list_get_applications    (ImApplicationList *list);

GDesktopAppInfo *       im_application_list_get_application     (ImApplicationList *list,
                                                                 const gchar       *id);

void                    im_application_list_set_status          (ImApplicationList *list,
                                                                 const gchar       *id,
                                                                 const gchar       *status);

#endif
