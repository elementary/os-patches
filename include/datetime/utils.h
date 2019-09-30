/*
 * Copyright 2010, 2014 Canonical Ltd.
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
 *   Michael Terry <michael.terry@canonical.com>
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#ifndef INDICATOR_DATETIME_UTILS_H
#define INDICATOR_DATETIME_UTILS_H

#include <glib.h>
#include <gio/gio.h> /* GSettings */

G_BEGIN_DECLS

/** \brief Returns true if the current locale prefers 12h display instead of 24h */
gboolean      is_locale_12h                        (void);

void          split_settings_location              (const char  * location,
                                                    char       ** zone,
                                                    char       ** name);

gchar *       get_timezone_name                    (const char  * timezone,
                                                    GSettings   * settings);

gchar *       get_beautified_timezone_name         (const char  * timezone,
                                                    const char  * saved_location);

gchar *       generate_full_format_string_at_time  (GDateTime   * now,
                                                    GDateTime   * then_begin,
                                                    GDateTime   * then_end);

/** \brief Translate the string based on LC_TIME instead of LC_MESSAGES.
           The intent of this is to let users set LC_TIME to override
           their other locale settings when generating time format string */
const char*   T_                                   (const char  * msg);

  
G_END_DECLS

#endif /* INDICATOR_DATETIME_UTILS_H */
