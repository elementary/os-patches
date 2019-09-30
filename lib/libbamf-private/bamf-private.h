/*
 * Copyright (C) 2011-2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#ifndef __BAMF_PRIVATE_H__
#define __BAMF_PRIVATE_H__

#include <libbamf-private/bamf-gdbus-generated.h>
#include <libbamf-private/bamf-gdbus-view-generated.h>

#define BAMF_DBUS_SERVICE_NAME (g_getenv ("BAMF_TEST_MODE") ? "org.ayatana.bamf.Test" : "org.ayatana.bamf")

#define BAMF_DBUS_BASE_PATH "/org/ayatana/bamf"
#define BAMF_DBUS_CONTROL_PATH BAMF_DBUS_BASE_PATH"/control"
#define BAMF_DBUS_MATCHER_PATH BAMF_DBUS_BASE_PATH"/matcher"

#define BAMF_DBUS_DEFAULT_TIMEOUT 500

#define BAMF_DEFAULT_ICON_SIZE 128
#define BAMF_DEFAULT_MINI_ICON_SIZE 24

/* GLib doesn't provide this by default */
#ifndef G_KEY_FILE_DESKTOP_KEY_FULLNAME
#define G_KEY_FILE_DESKTOP_KEY_FULLNAME "X-GNOME-FullName"
#endif

#define BAMF_APPLICATION_DEFAULT_ICON "application-default-icon"

#endif
