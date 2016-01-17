/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

Copyright 2010 Canonical Ltd.

Authors:
    Michael Terry <michael.terry@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __DATETIME_UTILS_H__
#define __DATETIME_UTILS_H__

#include <glib.h>
#include <gio/gio.h> /* GSettings */

G_BEGIN_DECLS

gboolean      is_locale_12h                        (void);

void          split_settings_location              (const char  * location,
                                                    char       ** zone,
                                                    char       ** name);

gchar *       get_current_zone_name                (const char  * location,
                                                    GSettings   * settings);

gchar*        join_date_and_time_format_strings    (const char  * date_fmt,
                                                    const char  * time_fmt);
/***
****
***/

const gchar * get_terse_time_format_string         (GDateTime   * time);

const gchar * get_terse_header_time_format_string  (void);

const gchar * get_full_time_format_string          (GSettings   * settings);

gchar *       generate_terse_format_string_at_time (GDateTime   * now,
                                                    GDateTime   * time);

gchar *       generate_full_format_string          (gboolean      show_day,
                                                    gboolean      show_date,
                                                    gboolean      show_year,
                                                    GSettings   * settings);

gchar *       generate_full_format_string_at_time  (GDateTime   * now,
                                                    GDateTime   * time,
                                                    GSettings   * settings);
  
G_END_DECLS

#endif
