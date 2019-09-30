/*
 * Copyright (C) 2012 Canonical Ltd
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
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 */


#ifndef __BAMF_XUTILS_H__
#define __BAMF_XUTILS_H__

#include <glib.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>

void  bamf_xutils_set_string_window_hint (Window xid, const char *atom_name, const char *value);
char* bamf_xutils_get_string_window_hint (Window xid, const char *atom_name);

void  bamf_xutils_get_window_class_hints (Window xid, char **class_instance_name, char **class_name);

#endif
