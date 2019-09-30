/*
 * Copyright (C) 2010-2013 Canonical Ltd
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
 *              Marco Trevisan (Treviño) <marco.trevisan@canonical.com>
 *
 */

#ifndef _BAMF_VIEW_PRIVATE_H_
#define _BAMF_VIEW_PRIVATE_H_

#include <gio/gio.h>

#include "bamf-application.h"
#include "bamf-tab.h"
#include "bamf-view.h"
#include "bamf-window.h"

#define CANCELLABLE(view) _bamf_view_get_cancellable (BAMF_VIEW (view))

BamfApplication * bamf_application_new          (const char *path);
BamfApplication * bamf_application_new_favorite (const char *favorite_path);
BamfTab         * bamf_tab_new                  (const char *path);
BamfWindow      * bamf_window_new               (const char *path);

void _bamf_view_set_path (BamfView *view, const char *dbus_path);

const char * _bamf_view_get_path (BamfView *view);

gboolean _bamf_view_remote_ready (BamfView *view);

void _bamf_view_reset_flags (BamfView *view);

void _bamf_view_set_cached_name (BamfView *view, const char *name);

void _bamf_view_set_cached_icon (BamfView *view, const char *icon);

void _bamf_view_set_closed (BamfView *view, gboolean closed);

GCancellable * _bamf_view_get_cancellable (BamfView *view);

#endif
