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

#ifndef __IM_MENU_H__
#define __IM_MENU_H__

#include <gio/gio.h>
#include "im-application-list.h"

#define IM_TYPE_MENU            (im_menu_get_type ())
#define IM_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IM_TYPE_MENU, ImMenu))
#define IM_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), IM_TYPE_MENU, ImMenuClass))
#define IM_IS_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IM_TYPE_MENU))
#define IM_IS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), IM_TYPE_MENU))
#define IM_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), IM_TYPE_MENU, ImMenuClass))

typedef struct _ImMenuClass   ImMenuClass;
typedef struct _ImMenu        ImMenu;
typedef struct _ImMenuPrivate ImMenuPrivate;

struct _ImMenuClass
{
  GObjectClass parent_class;
};

struct _ImMenu
{
  GObject parent_instance;
};

GType                   im_menu_get_type                                (void) G_GNUC_CONST;

ImApplicationList *     im_menu_get_application_list                    (ImMenu *menu);

gboolean                im_menu_export                                  (ImMenu           *menu,
                                                                         GDBusConnection  *connection,
                                                                         const gchar      *object_path,
                                                                         GError          **error);

void                    im_menu_prepend_section                         (ImMenu     *menu,
                                                                         GMenuModel *section);

void                    im_menu_append_section                          (ImMenu     *menu,
                                                                         GMenuModel *section);

void                    im_menu_insert_item_sorted                      (ImMenu    *menu,
                                                                         GMenuItem *item,
                                                                         gint       first,
                                                                         gint       last);

#endif
