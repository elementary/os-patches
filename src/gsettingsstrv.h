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

#ifndef __G_SETTINGS_STRV_H__
#define __G_SETTINGS_STRV_H__

#include <gio/gio.h>

gboolean        g_settings_strv_append_unique   (GSettings   *settings,
                                                 const gchar *key,
                                                 const gchar *item);

void            g_settings_strv_remove          (GSettings   *settings,
                                                 const gchar *key,
                                                 const gchar *item);

#endif
